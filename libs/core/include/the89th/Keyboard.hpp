#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Keys.hpp"
#include "Params.hpp"

namespace the89th
{

/** One of the KB 2000's two envelope generators, driving one channel's VCA.
    Straight-line segments; the panel gives attack, hold and release times.

    Push/Play: attack on the key, full while it is down, release when it lifts.
    Sustain: attack on the key, full for the hold time, then release, whatever
    the key does. A new note starts the attack from wherever the level is. */
class Envelope
{
public:
    enum class Stage { Idle, Attack, Hold, Sustain, Release };

    /** holdSeconds < 0 holds for ever (Sustain with the envelope off). */
    void configure (double attackSeconds, double holdSeconds, double releaseSeconds,
                    double sampleRate, bool sustainMode) noexcept
    {
        attackStep_  = 1.0 / std::max (1.0, attackSeconds * sampleRate);
        releaseStep_ = 1.0 / std::max (1.0, releaseSeconds * sampleRate);
        holdLength_  = holdSeconds < 0.0 ? -1L : std::lround (holdSeconds * sampleRate);
        sustainMode_ = sustainMode;
    }

    void trigger() noexcept { stage_ = Stage::Attack; }

    /** The key lifted. Sustain mode ignores it: the note runs its course. */
    void release() noexcept
    {
        if (! sustainMode_ && stage_ != Stage::Idle)
            stage_ = Stage::Release;
    }

    void kill() noexcept
    {
        stage_ = Stage::Idle;
        level_ = 0.0;
    }

    double next() noexcept
    {
        switch (stage_)
        {
            case Stage::Idle:
                level_ = 0.0;
                break;
            case Stage::Attack:
                level_ += attackStep_;
                if (level_ >= 1.0)
                {
                    level_    = 1.0;
                    stage_    = sustainMode_ ? Stage::Hold : Stage::Sustain;
                    holdLeft_ = holdLength_;
                }
                break;
            case Stage::Hold:
                if (holdLeft_ >= 0 && holdLeft_-- == 0)
                    stage_ = Stage::Release;
                break;
            case Stage::Sustain:
                break;
            case Stage::Release:
                level_ -= releaseStep_;
                if (level_ <= 0.0)
                {
                    level_ = 0.0;
                    stage_ = Stage::Idle;
                }
                break;
        }
        return level_;
    }

    double level() const noexcept { return level_; }
    bool   idle()  const noexcept { return stage_ == Stage::Idle; }

    /** Attack, hold or sustain: the note is still "on". */
    bool   on()    const noexcept { return stage_ != Stage::Idle && stage_ != Stage::Release; }

private:
    Stage  stage_ = Stage::Idle;
    double level_ = 0.0;
    double attackStep_ = 1.0, releaseStep_ = 1.0;
    long   holdLength_ = -1, holdLeft_ = -1;
    bool   sustainMode_ = false;
};

/** The KB 2000 as a controller over the machine, at host rate.

    The machine took each channel's pitch from an external clock: connecting
    one disconnected that channel's Pitch pots, and holding it high latched the
    machine and muted its output, which is what the keyboard's note-off did.
    So a channel the keyboard plays takes its pitch from the key, and while its
    envelope is silent it latches and mutes. Latched, the memory keeps what was
    playing, so the next note replays it at a new pitch.

    The engine calls, per chunk: process() with the input, which runs the
    envelopes, the vibrato, both synchros and the gate; then apply() on the
    parameters; then restartPending() to find which traversals to restart. It
    never touches audio itself: it hands back a gain per sample and settings. */
class Keyboard
{
public:
    /** With the envelope off, notes still fade in and out this fast. */
    static constexpr double kGateSeconds = 0.005;

    /** Vibrato modulator span at full pull: rate +-2 octaves, depth +-2 st. */
    static constexpr double kModRateOctaves = 2.0;
    static constexpr double kModDepthSemis  = 2.0;
    static constexpr double kMaxDepthSemis  = 2.0;

    /** Attack detector for Reverse Synchro: how fast its follower falls, and
        the shortest gap between two attacks it will report. */
    static constexpr double kFollowerRelease = 0.05;
    static constexpr double kMinAttackGap    = 0.05;

    void prepare (double hostRate) noexcept
    {
        fs_ = hostRate > 0.0 ? hostRate : 48000.0;
        reset();
        setParams (k_);
    }

    void reset() noexcept
    {
        notes_.clear();
        bend_ = 0.0;
        for (auto& v : voices_)
        {
            v.env.kill();
            v.note = -1;
            v.held = false;
            v.restart = false;
            v.vibPhase = v.vibEnv = 0.0;
            v.pos = k_.attackPoint;
        }
        for (auto& d : detect_)
            d = Detector {};
    }

    void setParams (const KeyboardParams& k) noexcept
    {
        const bool channelsChanged = k.channels != k_.channels;
        k_ = k;

        const bool sustain = k_.play == KeyPlay::Sustain;
        for (auto& v : voices_)
        {
            if (k_.envelope)
                v.env.configure (k_.attackSeconds, k_.holdSeconds, k_.releaseSeconds, fs_, sustain);
            else
                v.env.configure (kGateSeconds, -1.0, kGateSeconds, fs_, sustain);
        }

        // A side the keyboard stops playing goes back to its own controls; one
        // it starts playing picks up whatever keys are already down.
        if (channelsChanged)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& v = voices_[static_cast<std::size_t> (ch)];
                if (! drives (ch))
                {
                    v.env.kill();
                    v.held = false;
                    v.note = -1;
                }
            }
            allocate (-1, true);
        }
    }

    const KeyboardParams& params() const noexcept { return k_; }

    bool active() const noexcept { return k_.any(); }

    bool drives (int ch) const noexcept
    {
        return k_.channels == KeyChannels::Biphonic
            || (ch == 0 ? k_.channels == KeyChannels::Left : k_.channels == KeyChannels::Right);
    }

    // ─── Events, between chunks ─────────────────────────────────────────────

    void noteOn (int note) noexcept
    {
        notes_.press (note);
        allocate (note);
    }

    void noteOff (int note) noexcept
    {
        notes_.release (note);
        allocate (-1);
    }

    void pitchWheel (int value) noexcept { bend_ = keys::bendSemitones (value); }

    void allNotesOff() noexcept
    {
        notes_.clear();
        allocate (-1);
    }

    // ─── Per chunk ──────────────────────────────────────────────────────────

    /** Runs n host samples. in: the input per channel. gain: filled with each
        channel's wet gain. latched: whether each side's memory is held, since
        Reverse Synchro works on live input only. pitch: each side's current
        ratio, which Free synchro reading follows. internalRate and spanWords
        turn synchro speed into movement through memory. */
    void process (const float* const* in, float* const* gain, int n,
                  std::array<bool, 2> latched, std::array<double, 2> pitch,
                  double internalRate, double spanWords) noexcept
    {
        const double gateStep = 1.0 / (kGateSeconds * fs_);
        const double fall     = std::exp (-1.0 / (kFollowerRelease * fs_));
        const double thresh   = std::pow (10.0, k_.thresholdDb / 20.0);
        const long   gap      = std::lround (kMinAttackGap * fs_);
        const long   delay    = std::lround (std::max (0.0, k_.reverseDelaySeconds) * fs_);

        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            const int  c      = static_cast<int> (ch);
            auto&      v      = voices_[ch];
            auto&      d      = detect_[ch];
            const bool played = drives (c);
            const bool rev    = onSide (k_.reverseSynchro, c);
            const bool gated  = rev && k_.noiseGate;

            for (int i = 0; i < n; ++i)
            {
                double g = played ? v.env.next() : 1.0;

                if (rev)
                {
                    // Peak follower with hysteresis: an attack is the level
                    // crossing the threshold after having fallen to half of it.
                    const double x = std::fabs (static_cast<double> (in[ch][i]));
                    d.level = std::max (x, d.level * fall);
                    if (d.level < 0.5 * thresh)
                        d.armed = true;
                    ++d.sinceAttack;
                    if (d.armed && d.level >= thresh && d.sinceAttack >= gap && ! latched[ch])
                    {
                        d.armed = false;
                        d.sinceAttack = 0;
                        d.countdown = delay;
                    }
                    if (d.countdown >= 0 && d.countdown-- == 0)
                        v.restart = true;
                }

                if (gated)
                {
                    const double target = d.level >= 0.5 * thresh ? 1.0 : 0.0;
                    d.gate += std::clamp (target - d.gate, -gateStep, gateStep);
                    g *= d.gate;
                }

                gain[ch][i] = static_cast<float> (g);
            }

            if (! played)
                continue;

            const double dt = static_cast<double> (n) / fs_;

            // The vibrato's modulator rises while the note is on and falls
            // once it is released.
            if (k_.vibrato)
            {
                const double t = v.env.on() ? k_.vibAttackSeconds : k_.vibReleaseSeconds;
                const double step = dt / std::max (t, 1e-3);
                v.vibEnv = std::clamp (v.vibEnv + (v.env.on() ? step : -step), 0.0, 1.0);
                v.vibPhase += 2.0 * M_PI * vibratoRate (v) * dt;
                if (v.vibPhase > 2.0 * M_PI)
                    v.vibPhase -= 2.0 * M_PI;
            }

            // Memory Synchro: the reading position moves through the memory at
            // the set speed and loops from the end point back to the return.
            if (onSide (k_.memorySynchro, c) && ! v.env.idle() && spanWords > 0.0)
            {
                const double rate = k_.speed > 0.0 ? k_.speed : pitch[ch];
                v.pos += rate * dt * internalRate / spanWords;
                if (v.pos >= k_.endPoint)
                {
                    if (k_.returnPoint < k_.endPoint)
                    {
                        const double loop = k_.endPoint - k_.returnPoint;
                        v.pos = k_.returnPoint + std::fmod (v.pos - k_.endPoint, loop);
                        v.restart = true;
                    }
                    else
                    {
                        v.pos = k_.endPoint;
                    }
                }
            }
        }
    }

    /** Puts the keyboard's settings into the machine's: pitch from the key,
        latch while silent, the synchro's region, the added delay. */
    void apply (EngineParams& p) const noexcept
    {
        ChannelParams* cs[2] = { &p.left, &p.right };
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            const int c = static_cast<int> (ch);
            if (! drives (c))
                continue;

            const auto& v  = voices_[ch];
            auto&       cp = *cs[ch];
            const int note = v.note >= 0 ? v.note : k_.root;
            cp.pitchRatio = keys::ratio (note, k_.root, bend_ + k_.trimCents / 100.0);

            if (v.env.idle())
                cp.freeze = true;

            const double lo  = std::min (cp.crosspoint1, cp.crosspoint2);
            const double hi  = std::max (cp.crosspoint1, cp.crosspoint2);
            const double len = hi - lo;
            const bool   rev = cp.crosspoint1 > cp.crosspoint2;

            double newLo = lo;
            if (onSide (k_.memorySynchro, c))
            {
                // Position 0 is the oldest sound in memory, the deepest delay.
                // Reading forward walks toward the newest, so the segment
                // starts at the region's deep bound.
                const double at = 1.0 - std::clamp (v.pos, 0.0, 1.0);
                newLo = std::clamp (at - len, 0.0, 1.0 - len);
            }
            else if (k_.addedDelay > 0.0)
            {
                newLo = lo + k_.addedDelay * (1.0 - hi);
            }

            cp.crosspoint1 = rev ? newLo + len : newLo;
            cp.crosspoint2 = rev ? newLo       : newLo + len;
        }
    }

    /** Semitones the keyboard's vibrato adds to a channel right now. */
    double vibratoSemitones (int ch) const noexcept
    {
        if (! k_.vibrato || ! drives (ch))
            return 0.0;

        const auto& v = voices_[static_cast<std::size_t> (ch)];
        const double depth = std::clamp (k_.vibDepth + k_.vibModDepth * v.vibEnv * kModDepthSemis,
                                         0.0, kMaxDepthSemis);
        const double sharp = std::clamp (k_.vibSharpness + k_.vibModSharpness * v.vibEnv, 0.0, 1.0);

        // Sharpness squares the sine up by driving it into a soft clip.
        const double s = std::sin (v.vibPhase);
        const double k = 12.0 * sharp;
        const double w = k < 1e-3 ? s : std::tanh (k * s) / std::tanh (k);
        return depth * w;
    }

    /** True once per restart the traversal owes: a note's attack, a synchro
        loop, or an attack in the input under Reverse Synchro. */
    bool takeRestart (int ch) noexcept
    {
        auto& r = voices_[static_cast<std::size_t> (ch)].restart;
        const bool was = r;
        r = false;
        return was;
    }

    /** The key a channel is playing, -1 if none sounds, -2 if not played. */
    int sounding (int ch) const noexcept
    {
        if (! drives (ch))
            return -2;
        const auto& v = voices_[static_cast<std::size_t> (ch)];
        return v.env.idle() ? -1 : v.note;
    }

    /** Memory Synchro's reading position, 0 oldest to 1 newest. */
    double synchroPosition (int ch) const noexcept { return voices_[static_cast<std::size_t> (ch)].pos; }

private:
    struct Voice
    {
        Envelope env;
        int    note = -1;
        bool   held = false;
        bool   restart = false;
        double vibPhase = 0.0, vibEnv = 0.0;
        double pos = 0.0;
    };

    struct Detector
    {
        double level = 0.0;
        double gate  = 0.0;
        bool   armed = true;
        long   sinceAttack = 1L << 30;
        long   countdown = -1;
    };

    double vibratoRate (const Voice& v) const noexcept
    {
        return k_.vibRateHz * std::exp2 (k_.vibModRate * v.vibEnv * kModRateOctaves);
    }

    /** Hands the held keys to the channels. Left or Right: the newest key.
        Biphonic: the two newest, lower on the left, higher on the right; one
        key plays on both. A channel whose key is the one just pressed starts
        a note; one that only changes key glides there without restarting.
        pickUp starts a note on any side that has a key but no note yet. */
    void allocate (int pressed, bool pickUp = false) noexcept
    {
        std::array<int, 2> want { -1, -1 };
        if (notes_.active())
        {
            if (k_.channels == KeyChannels::Biphonic)
            {
                const int a = notes_.recent (0);
                const int b = notes_.held() > 1 ? notes_.recent (1) : a;
                want = { std::min (a, b), std::max (a, b) };
            }
            else
            {
                want = { notes_.current(), notes_.current() };
            }
        }

        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            if (! drives (static_cast<int> (ch)))
                continue;

            auto& v = voices_[ch];
            if (want[ch] < 0)
            {
                v.held = false;
                v.env.release();
                continue;
            }

            const bool start = (pressed >= 0 && want[ch] == pressed && (v.note != pressed || ! v.held))
                            || (pickUp && ! v.held);
            v.note = want[ch];
            v.held = true;
            if (start)
            {
                v.env.trigger();
                v.pos     = k_.attackPoint;
                v.restart = true;
            }
        }
    }

    KeyboardParams k_ {};
    keys::NoteStack notes_;
    double bend_ = 0.0;
    double fs_   = 48000.0;
    std::array<Voice, 2>    voices_ {};
    std::array<Detector, 2> detect_ {};
};

} // namespace the89th
