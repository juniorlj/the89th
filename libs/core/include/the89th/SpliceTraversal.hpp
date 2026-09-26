#pragma once

#include <algorithm>
#include <cmath>
#include <functional>

namespace the89th
{

/** Region traversal and splice crossfade, worked entirely in the delay domain.

    delay is how far a read head sits behind the write head. With the write head
    advancing at writeRate and a read head at signedRate:

        read_pos    = write_pos - delay
        d(delay)/dt = writeRate - signedRate

    Two things fall out of that one line rather than needing modes:

    Reverse. signedRate carries the pitch magnitude and takes its sign from
    crosspoint order. Put crosspoint 1 deeper than crosspoint 2 and the sign
    flips, the read head walks backwards through memory, and reverse costs no
    branch. At ratio 1.0 reversed the step is 2.0, so the head retreats through
    memory at exactly playback speed: backwards at original pitch.

    Freeze. writeRate drops to 0, so the step becomes -signedRate. The write
    head stops but the read head keeps moving, which loops the crosspoint region
    instead of parking on a single sample. Latch is one number, not a mode.

    Ratio 1.0 forward gives a step of exactly 0. The head never reaches a bound,
    no splice ever fires, and the secondary stays muted, so the delay path is
    transparent by construction rather than by tolerance.

    Holds no audio and touches no buffer, so every splice decision can be tested
    on its own. */
class SpliceTraversal
{
public:
    struct Tap
    {
        double delaySamples = 0.0;
        float  gain         = 0.0f;
    };

    /** A degenerate region would splice every sample and never advance. */
    static constexpr double kMinRegion = 8.0;

    void setCrossfadeLength (int samples) noexcept
    {
        crossfade_ = std::max (1, samples);
    }

    void setRegion (double xp1Delay, double xp2Delay) noexcept
    {
        xp1_ = xp1Delay;
        xp2_ = xp2Delay;

        lo_ = std::min (xp1_, xp2_);
        hi_ = std::max (xp1_, xp2_);
        if (hi_ - lo_ < kMinRegion)
            hi_ = lo_ + kMinRegion;
        length_ = hi_ - lo_;

        reverse_ = xp1_ > xp2_;
        updateStep();

        if (needsPlacement_)
        {
            primary_ = secondary_ = entryBound();
            fadePos_ = -1;
            needsPlacement_ = false;
        }
        else if (fadePos_ < 0 && (primary_ < lo_ || primary_ > hi_))
        {
            // Crosspoints are pots: they move under the head. A head the region
            // has left behind splices back in with the same crossfade that ends
            // a traversal. Snapping it onto the new bound would be a jump in
            // read position, which is a click.
            spliceTo (entryBound());
        }
    }

    void setRatio (double pitchRatio) noexcept
    {
        ratio_ = std::abs (pitchRatio);
        updateStep();
    }

    /** 1.0 while writing, 0.0 when the memory latch is engaged. */
    void setWriteRate (double r) noexcept
    {
        writeRate_ = r;
        updateStep();
    }

    void reset() noexcept
    {
        needsPlacement_ = true;
        fadePos_        = -1;
        launched_       = false;
        pendingLaunch_  = false;
        restartAfterFade_ = false;
        primary_ = secondary_ = entryBound();
    }

    /** Put the head at a specific delay, for handing over from the fixed delay
        head when the latch engages in delay mode: the read position carries
        straight across, so nothing jumps. */
    void placeAt (double delay) noexcept
    {
        primary_ = secondary_ = std::clamp (delay, lo_, hi_);
        fadePos_        = -1;
        launched_       = false;
        pendingLaunch_  = false;
        restartAfterFade_ = false;
        needsPlacement_ = false;
    }

    /** Start the traversal again from its entry bound, crossfading as any
        splice does. The keyboard does this on a note's attack and Reverse
        Synchro on the input's, so a segment begins with the sound rather than
        wherever the free-running traversal happened to be. A splice already
        heading there covers it; one heading elsewhere finishes first. */
    void restart() noexcept
    {
        if (fadePos_ >= 0)
        {
            restartAfterFade_ = ! (pendingLaunch_ && std::equal_to<double> {} (secondary_, entryBound()));
            return;
        }
        spliceTo (entryBound());
    }

    /** True for the one sample on which a splice began. The voice uses it to
        move the incoming head to a better-matching spot before it is heard. */
    bool justLaunched() const noexcept { return launched_; }

    /** Nudge the incoming head. Only meaningful right after a launch. */
    void shiftIncoming (double delta) noexcept { secondary_ += delta; }

    /** Curve blend for the fade: 0 gives equal power, 1 gives equal gain. */
    void setFadeShape (float correlation) noexcept
    {
        shape_ = correlation < 0.0f ? 0.0f : (correlation > 1.0f ? 1.0f : correlation);
    }

    int    activeFade() const noexcept { return activeFade_; }
    double primaryDelay() const noexcept { return primary_; }

    void advance() noexcept
    {
        // A splice started by a region move shows as launched after the next
        // step, so the voice's Xing still gets to place its incoming head.
        launched_      = pendingLaunch_;
        pendingLaunch_ = false;
        primary_ += step_;

        // The standby head only moves while it is being faded in. Letting it
        // free-run would drift it arbitrarily far from the region between
        // splices; it is silent then, but the state stops meaning anything.
        if (fadePos_ >= 0)
            secondary_ += step_;

        if (fadePos_ >= 0)
        {
            if (++fadePos_ >= activeFade_)
            {
                // Clamped as a backstop: a region small enough that the fade
                // eats most of it would otherwise let the handover position
                // ratchet a little further out on every splice.
                primary_ = std::clamp (secondary_, lo_, hi_);
                fadePos_ = -1;
                if (restartAfterFade_)
                {
                    restartAfterFade_ = false;
                    spliceTo (entryBound());
                }
            }
            return;
        }

        if (step_ == 0.0)
            return;  // ratio 1.0 forward while writing: nothing ever splices

        const double toExit        = (step_ > 0.0) ? (hi_ - primary_) : (primary_ - lo_);
        const double samplesToExit = toExit / std::abs (step_);

        activeFade_ = fadeLengthForStep();

        if (samplesToExit > static_cast<double> (activeFade_))
            return;

        if (activeFade_ <= 0)
        {
            primary_ = wrapped (primary_);  // region too short to fade across
        }
        else
        {
            // Launch the incoming head ON the entry bound, not one region length
            // behind the outgoing one.
            //
            // A plain region-length offset puts the incoming head outside memory
            // whenever the region spans most of it: launching it 96 samples early
            // asks for a negative delay, clampDelay pins it to the minimum, and a
            // pinned head tracks the write pointer at 1x instead of the pitch
            // ratio. The fade then mixes untransposed signal into the splice,
            // which is audible and measures as a discontinuity. Offsetting by the
            // fade's own travel keeps both heads inside the region for the whole
            // crossfade, at the cost of each traversal being shorter than the
            // region by that same travel.
            secondary_ = wrapped (primary_) + activeFade_ * step_;
            fadePos_   = 0;
            launched_  = true;
        }
    }

    Tap primary() const noexcept
    {
        return { primary_, fadePos_ < 0 ? 1.0f : fadeGain (true) };
    }

    Tap secondary() const noexcept
    {
        return { secondary_, fadePos_ < 0 ? 0.0f : fadeGain (false) };
    }

    bool   splicing()   const noexcept { return fadePos_ >= 0; }
    bool   reversed()   const noexcept { return reverse_; }
    double signedRate() const noexcept { return signedRate_; }
    double step()       const noexcept { return step_; }
    double regionLo()   const noexcept { return lo_; }
    double regionHi()   const noexcept { return hi_; }
    double regionLength() const noexcept { return length_; }

private:
    void spliceTo (double delay) noexcept
    {
        activeFade_ = fadeLengthForStep();
        if (activeFade_ <= 0)
        {
            primary_ = delay;  // region too short to fade across
            return;
        }

        secondary_     = delay;
        fadePos_       = 0;
        pendingLaunch_ = true;
    }

    void updateStep() noexcept
    {
        signedRate_ = (reverse_ ? -1.0 : 1.0) * ratio_;
        step_       = writeRate_ - signedRate_;
    }

    double wrapped (double d) const noexcept
    {
        return d - (step_ > 0.0 ? length_ : -length_);
    }

    /** Where a steady-state traversal begins, which is the bound opposite the
        one the head drifts toward.

        Note this is the sign of the step, not crosspoint order. A ratio below
        1.0 forward drifts deeper and exits at the far bound, so it has to start
        at the near one. Placing it on crosspoint 2 unconditionally would put a
        ratio-0.5 head on the bound it is about to leave, splicing on the first
        sample into memory that has not been written yet. */
    double entryBound() const noexcept
    {
        if (step_ == 0.0)
            return std::clamp (xp2_, lo_, hi_);

        return step_ > 0.0 ? lo_ : hi_;
    }

    /** A fast step covers a lot of ground per sample, so a short region has to
        cap the fade or it would still be fading when the next splice is due.

        The cap is a quarter of the traversal, not half. Handover leaves the head
        one fade's travel inside the entry bound, so a half-region fade would put
        it back in trigger range on the very next sample and the splice would
        never make progress. A quarter leaves three fades of clearance. */
    int fadeLengthForStep() const noexcept
    {
        const double travel = std::abs (step_);
        if (travel <= 0.0)
            return crossfade_;

        const double byRegion = 0.25 * length_ / travel;
        return static_cast<int> (std::min (static_cast<double> (crossfade_), byRegion));
    }

    /** Equal power suits uncorrelated material and equal gain suits matched
        material; summing two in-phase heads at equal power bumps the level by
        3 dB. The shape blends between them by how well the join matched. */
    float fadeGain (bool outgoing) const noexcept
    {
        constexpr float kHalfPi = 1.57079632679489662f;
        const float t  = static_cast<float> (fadePos_) / static_cast<float> (activeFade_);
        const float u  = outgoing ? 1.0f - t : t;
        const float ep = std::sin (u * kHalfPi);
        return (1.0f - shape_) * ep + shape_ * u;
    }

    double xp1_ = 0.0, xp2_ = 0.0;
    double lo_ = 0.0, hi_ = kMinRegion, length_ = kMinRegion;
    double primary_ = 0.0, secondary_ = 0.0;

    double ratio_      = 1.0;
    double writeRate_  = 1.0;
    double signedRate_ = 1.0;
    double step_       = 0.0;
    bool   reverse_    = false;

    int  crossfade_  = 96;
    int  activeFade_ = 96;
    int  fadePos_    = -1;  // -1 when not splicing
    bool needsPlacement_ = true;
    bool launched_   = false;
    bool pendingLaunch_ = false;
    bool restartAfterFade_ = false;
    float shape_     = 0.0f;
};

} // namespace the89th
