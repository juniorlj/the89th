#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "Version.h"

namespace
{
juce::String ratioToText (float v, int)
{
    // Semitones read better than a bare ratio when the control is logarithmic.
    const auto semis = 12.0f * std::log2 (v);
    return juce::String (v, 3) + "x  (" + juce::String (semis, 2) + " st)";
}

juce::String msText (double ms)
{
    // juce::String (double, 0) means "full precision", not "no decimals".
    return (ms < 100.0 ? juce::String (ms, 1) : juce::String (juce::roundToInt (ms))) + " ms";
}

juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
juce::String centsText (float v, int)   { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; }
juce::String semitoneText (float v, int) { return juce::String (v, 2) + " st"; }
juce::String hertzText (float v, int)    { return juce::String (v, v < 1.0f ? 2 : 1) + " Hz"; }

constexpr int kMinDelay = the89th::DelayMemory::kMinDelay;
constexpr int kEndGuard = the89th::DelayMemory::kEndGuard;
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout The89thProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Lives in the parameter list so hosts that ignore editor chrome (or keep a
    // stale mapped binary's editor) still show which build is loaded.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::build, 1 }, "Build",
        StringArray { the89th_version::banner() }, 0,
        AudioParameterChoiceAttributes{}.withAutomatable (false)));

    // ─── Global: the hardware's switches ────────────────────────────────────
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::stereo, 2 }, "Stereo",
        StringArray { "True stereo", "Quasi-stereo" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::range, 2 }, "Range",
        StringArray { "Long", "Short" }, 0));

    // 20 kHz needs the whole converter, so only quasi-stereo reaches it; in true
    // stereo it falls back to 10 kHz.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::bandwidth, 1 }, "Bandwidth",
        StringArray { "5 kHz", "10 kHz", "20 kHz (quasi-stereo)" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    // ─── Per channel ────────────────────────────────────────────────────────
    // Positions in memory read as milliseconds, which depend on the current
    // bandwidth, stereo layout and range, so the text functions ask the
    // processor rather than bake in one clock.
    // With Sync on, the same knobs step through note values; a value longer
    // than the memory holds says so.
    auto positionText = [this] (float v, bool isDelay)
    {
        const double full = static_cast<double> (readout_.words.load() - kEndGuard - kMinDelay);
        const double span = isDelay && readout_.shortRange.load() ? full / 10.0 : full;

        if (! readout_.sync.load())
            return msText ((kMinDelay + v * span) * readout_.msPerWord.load());

        const int step = the89th::musical::syncStep (v);
        const double ms = 1000.0 * the89th::musical::syncSeconds (step, readout_.bpm.load());
        const double maxMs = (kMinDelay + span) * readout_.msPerWord.load();
        return juce::String (the89th::musical::syncName (step)) + (ms > maxMs ? " > MAX" : "");
    };
    auto crosspointMs = [positionText] (float v, int) { return positionText (v, false); };
    auto delayMs      = [positionText] (float v, int) { return positionText (v, true); };

    // Shows where the pitch lands once Snap has had its say.
    auto pitchText = [this] (float v, int)
    {
        const auto scale = static_cast<the89th::musical::Scale> (readout_.snap.load());
        return ratioToText (static_cast<float> (the89th::musical::pitchRatio (v, scale, 0.0)), 0);
    };

    // 0.25 to 2.0 is the hardware's span: two octaves down to one up. Skewed so
    // unity sits mid-travel rather than three quarters of the way along.
    auto pitchRange = NormalisableRange<float> (0.25f, 2.0f);
    pitchRange.setSkewForCentre (1.0f);

    for (int c = 0; c < 2; ++c)
    {
        const auto& id  = pid::channel[c];
        const String side = c == 0 ? "L " : "R ";
        // Left keeps the Phase 0 IDs and versions; everything new is version 2.
        const int v = c == 0 ? 1 : 2;

        // Each side has its own Delay / Pitch-Shifter / Memory Latch buttons.
        // The left keeps the IDs these had when they were global.
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id.mode, c == 0 ? 2 : 5 }, side + "Mode",
            StringArray { "Delay", "Pitch" }, 1));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { id.freeze, c == 0 ? 1 : 5 }, side + "Memory latch", false));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.delay, 2 }, side + "Delay",
            NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (delayMs)));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.pitch, v }, side + "Pitch",
            pitchRange, 1.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (pitchText)));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.crosspoint1, v }, side + "Crosspoint 1",
            NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (crosspointMs)));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.crosspoint2, v }, side + "Crosspoint 2",
            NormalisableRange<float> (0.0f, 1.0f), 1.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (crosspointMs)));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.feedback, v }, side + "Feedback",
            NormalisableRange<float> (0.0f, 0.99f), 0.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

        // Depth and speed ranges are not published; later units had both pots.
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.vibratoDepth, 2 }, side + "Vibrato depth",
            NormalisableRange<float> (0.0f, 2.0f), 0.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (semitoneText)));

        auto rateRange = NormalisableRange<float> (0.1f, 10.0f);
        rateRange.setSkewForCentre (2.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.vibratoRate, 2 }, side + "Vibrato speed",
            rateRange, 5.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (hertzText)));

        // Modern, version 3, neutral by default.
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id.fine, 3 }, side + "Fine",
            NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (centsText)));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { id.vibratoShape, 3 }, side + "Vibrato shape",
            StringArray { "Sine", "Square" }, 0));
    }

    // ─── Global: modern controls, version 3, neutral by default ─────────────
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::link, 3 }, "Link", false));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::fbRoute, 3 }, "Feedback routing",
        StringArray { "Normal", "Cross", "Sum" }, 0));

    auto lowCutRange = NormalisableRange<float> (20.0f, 2000.0f);
    lowCutRange.setSkewForCentre (200.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::lowCut, 3 }, "Low cut",
        lowCutRange, 20.0f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction ([] (float v, int)
        {
            return v <= 20.5f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " Hz";
        })));

    auto highCutRange = NormalisableRange<float> (1000.0f, 20000.0f);
    highCutRange.setSkewForCentre (5000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::highCut, 3 }, "High cut",
        highCutRange, 20000.0f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction ([] (float v, int)
        {
            return v >= 19999.5f ? juce::String ("Off") : juce::String (v / 1000.0f, 1) + " kHz";
        })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::drive, 3 }, "Drive",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::snap, 3 }, "Snap",
        StringArray { "Off", "Chromatic", "Major", "Minor", "Pentatonic" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::sync, 3 }, "Sync", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::scrubDepth, 3 }, "Scrub depth",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction (percentText)));

    auto scrubRateRange = NormalisableRange<float> (0.05f, 10.0f);
    scrubRateRange.setSkewForCentre (0.5f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::scrubRate, 3 }, "Scrub speed",
        scrubRateRange, 0.5f,
        AudioParameterFloatAttributes{}.withStringFromValueFunction (hertzText)));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::scrubMode, 3 }, "Scrub mode",
        StringArray { "LFO", "Random" }, 0));

    // ─── The KB 2000 ────────────────────────────────────────────────────────
    // MIDI plays the keyboard controller sold with the machine, laid out as its
    // panel: pitch ratio settings, envelope, vibrato, Memory Synchro and
    // Reverse Synchro. The panel gives no scales, so the ranges here are
    // choices. Off leaves the machine exactly as it is.
    auto seconds = [] (float v, int) { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                                                        : juce::String (v, 2) + " s"; };
    auto bipolar = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto timeRange = [] (float lo, float hi, float centre)
    {
        auto r = NormalisableRange<float> (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    auto addFloat = [&layout] (const char* id, const String& name, NormalisableRange<float> range, float def,
                               std::function<String (float, int)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 4 }, name, range, def,
            AudioParameterFloatAttributes{}.withStringFromValueFunction (std::move (text))));
    };
    const StringArray sides { "Off", "Left", "Right", "Both" };

    // Pitch ratio settings.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::keys, 4 }, "KB channels",
        StringArray { "Off", "Left", "Right", "Biphonic" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { pid::keysRoot, 4 }, "KB root", 24, 96, 60,
        AudioParameterIntAttributes{}.withStringFromValueFunction ([] (int n, int)
        {
            // Middle C (60) as C3, the convention Henke's re-creation anchors on.
            return juce::MidiMessage::getMidiNoteName (n, true, true, 3);
        })));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::kbPlay, 4 }, "KB play", StringArray { "Push/Play", "Sustain" }, 0));
    addFloat (pid::kbTrim,  "KB trimmer", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, centsText);
    addFloat (pid::kbSlope, "KB slope", timeRange (0.0f, 2.0f, 0.2f), 0.0f,
              [] (float v, int) { return v < 0.0005f ? juce::String ("Off") : (v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"); });
    addFloat (pid::kbAdded, "KB added delay", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentText);

    // Envelope.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::kbEnv, 4 }, "KB envelope", false));
    addFloat (pid::kbAttack,  "KB attack",  timeRange (0.001f, 2.0f, 0.1f), 0.01f, seconds);
    addFloat (pid::kbHold,    "KB hold",    timeRange (0.0f,   5.0f, 0.5f), 0.5f,  seconds);
    addFloat (pid::kbRelease, "KB release", timeRange (0.005f, 5.0f, 0.3f), 0.3f,  seconds);

    // Vibrato.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::kbVib, 4 }, "KB vibrato", false));
    addFloat (pid::kbVibRate,  "KB vibrato frequency", timeRange (0.1f, 12.0f, 3.0f), 5.0f, hertzText);
    addFloat (pid::kbVibSharp, "KB vibrato sharpness", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentText);
    addFloat (pid::kbVibDepth, "KB vibrato depth", NormalisableRange<float> (0.0f, 2.0f), 0.5f, semitoneText);
    addFloat (pid::kbVibModRate,  "KB modulator to frequency", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar);
    addFloat (pid::kbVibModSharp, "KB modulator to sharpness", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar);
    addFloat (pid::kbVibModDepth, "KB modulator to depth", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar);
    addFloat (pid::kbVibAttack,  "KB modulator attack",  timeRange (0.0f, 5.0f, 0.5f), 0.5f, seconds);
    addFloat (pid::kbVibRelease, "KB modulator release", timeRange (0.0f, 5.0f, 0.5f), 0.5f, seconds);

    // Memory Synchro. Points read as a place in the latched memory, in ms from
    // its oldest end; speed 0 is Free, reading at the pitch.
    auto pointText = [this] (float v, int)
    {
        return msText (v * static_cast<double> (readout_.words.load()) * readout_.msPerWord.load());
    };
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::kbSynchro, 4 }, "KB memory synchro", sides, 0));
    addFloat (pid::kbAttackPt, "KB attack point", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pointText);
    addFloat (pid::kbReturnPt, "KB return point", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pointText);
    addFloat (pid::kbEndPt,    "KB end point",    NormalisableRange<float> (0.0f, 1.0f), 1.0f, pointText);
    addFloat (pid::kbSpeed,    "KB speed", timeRange (0.0f, 2.0f, 1.0f), 1.0f,
              [] (float v, int) { return v < 0.005f ? juce::String ("Free") : juce::String (v, 2) + "x"; });

    // Reverse Synchro.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::kbReverse, 4 }, "KB reverse synchro", sides, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::kbGate, 4 }, "KB noise gate", false));
    addFloat (pid::kbThresh,   "KB threshold", NormalisableRange<float> (-60.0f, 0.0f), -30.0f,
              [] (float v, int) { return juce::String (juce::roundToInt (v)) + " dB"; });
    addFloat (pid::kbRevDelay, "KB added delay (reverse)", timeRange (0.0f, 1.0f, 0.15f), 0.0f, seconds);

    // Not automatable: a host automation lane or a "randomise" should never be
    // able to wipe every setting and the memory mid-song. The panel button
    // still works.
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { pid::init, 1 }, "Init", false,
        AudioParameterBoolAttributes{}.withAutomatable (false)));

    return layout;
}

The89thProcessor::The89thProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "THE89TH", createLayout())
{
    for (int c = 0; c < 2; ++c)
    {
        const auto& id = pid::channel[c];
        auto& raw = ch_[static_cast<std::size_t> (c)];
        raw.mode     = apvts.getRawParameterValue (id.mode);
        raw.freeze   = apvts.getRawParameterValue (id.freeze);
        raw.delay    = apvts.getRawParameterValue (id.delay);
        raw.pitch    = apvts.getRawParameterValue (id.pitch);
        raw.xp1      = apvts.getRawParameterValue (id.crosspoint1);
        raw.xp2      = apvts.getRawParameterValue (id.crosspoint2);
        raw.feedback = apvts.getRawParameterValue (id.feedback);
        raw.vibDepth = apvts.getRawParameterValue (id.vibratoDepth);
        raw.vibRate  = apvts.getRawParameterValue (id.vibratoRate);
        raw.fine     = apvts.getRawParameterValue (id.fine);
        raw.vibShape = apvts.getRawParameterValue (id.vibratoShape);
    }

    stereo_    = apvts.getRawParameterValue (pid::stereo);
    range_     = apvts.getRawParameterValue (pid::range);
    bandwidth_ = apvts.getRawParameterValue (pid::bandwidth);
    mix_       = apvts.getRawParameterValue (pid::mix);

    link_       = apvts.getRawParameterValue (pid::link);
    fbRoute_    = apvts.getRawParameterValue (pid::fbRoute);
    lowCut_     = apvts.getRawParameterValue (pid::lowCut);
    highCut_    = apvts.getRawParameterValue (pid::highCut);
    drive_      = apvts.getRawParameterValue (pid::drive);
    snap_       = apvts.getRawParameterValue (pid::snap);
    sync_       = apvts.getRawParameterValue (pid::sync);
    scrubDepth_ = apvts.getRawParameterValue (pid::scrubDepth);
    scrubRate_  = apvts.getRawParameterValue (pid::scrubRate);
    scrubMode_  = apvts.getRawParameterValue (pid::scrubMode);
    std::size_t kbCount = 0;
    for (const char* id : { pid::keys, pid::keysRoot, pid::kbPlay, pid::kbTrim, pid::kbSlope, pid::kbAdded,
                            pid::kbEnv, pid::kbAttack, pid::kbHold, pid::kbRelease,
                            pid::kbVib, pid::kbVibRate, pid::kbVibSharp, pid::kbVibDepth,
                            pid::kbVibModRate, pid::kbVibModSharp, pid::kbVibModDepth,
                            pid::kbVibAttack, pid::kbVibRelease,
                            pid::kbSynchro, pid::kbAttackPt, pid::kbReturnPt, pid::kbEndPt, pid::kbSpeed,
                            pid::kbReverse, pid::kbGate, pid::kbThresh, pid::kbRevDelay })
    {
        jassert (kbCount < kb_.size());
        kb_[kbCount++] = { id, apvts.getRawParameterValue (id) };
    }

    apvts.addParameterListener (pid::init, this);
    updateReadout();
}

The89thProcessor::~The89thProcessor()
{
    apvts.removeParameterListener (pid::init, this);
    cancelPendingUpdate();
}

void The89thProcessor::parameterChanged (const juce::String& id, float value)
{
    // Called from whichever thread moved the control, possibly the audio one,
    // so do nothing here but hand off.
    if (id == pid::init && value > 0.5f)
        triggerAsyncUpdate();
}

void The89thProcessor::handleAsyncUpdate()
{
    for (auto* p : getParameters())
    {
        auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);
        if (withID == nullptr
            || withID->paramID == pid::init
            || withID->paramID == pid::build)
            continue;

        p->setValueNotifyingHost (p->getDefaultValue());
    }

    // Clear the machine as well as the panel: a half-full delay line is state
    // the user cannot see, and leaving it makes Init feel like it half worked.
    // The audio thread owns the engine, so ask rather than reach in.
    resetRequested_.store (true);

    if (auto* initParam = apvts.getParameter (pid::init))
        initParam->setValueNotifyingHost (0.0f);
}

void The89thProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock);
    updateReadout();
    engine_.setParams (readParams());
    engine_.reset();

    // Conversion latency only. The delay the engine imposes is the effect. The
    // engine keeps it the same at every clock, so it is only ever set here: a
    // change mid-session would make the host restart the plugin and empty its
    // memory.
    setLatencySamples (juce::roundToInt (engine_.latencySamples()));
}

bool The89thProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

the89th::EngineParams The89thProcessor::readParams() const
{
    auto get = [] (std::atomic<float>* a, float fallback) { return a != nullptr ? a->load() : fallback; };
    namespace mus = the89th::musical;

    const bool linked  = get (link_, 0.0f) > 0.5f;
    const bool synced  = readout_.sync.load();
    const auto scale   = static_cast<mus::Scale> (readout_.snap.load());

    // Synced knobs pick a note; this turns its length into the normalised
    // position the core takes, clamped to what the memory holds.
    const double msPerWord = readout_.msPerWord.load();
    const double full      = static_cast<double> (readout_.words.load() - kEndGuard - kMinDelay);
    auto synced01 = [&] (float knob, double span)
    {
        const int step = mus::syncStep (knob);
        if (step == 0)
            return 0.0;
        const double words = 1000.0 * mus::syncSeconds (step, readout_.bpm.load()) / msPerWord;
        return std::clamp ((words - kMinDelay) / span, 0.0, 1.0);
    };

    the89th::EngineParams p;
    the89th::ChannelParams* out[2] = { &p.left, &p.right };

    for (std::size_t c = 0; c < 2; ++c)
    {
        // Linked, channel 2 takes every per-channel control from channel 1.
        const auto& raw = ch_[linked ? 0 : c];
        auto& cp = *out[c];
        cp.mode         = get (raw.mode, 1.0f) < 0.5f ? the89th::Mode::Delay : the89th::Mode::Pitch;
        cp.freeze       = get (raw.freeze, 0.0f) > 0.5f;
        cp.delay        = get (raw.delay, 0.5f);
        cp.pitchRatio   = mus::pitchRatio (get (raw.pitch, 1.0f), scale, get (raw.fine, 0.0f));
        cp.crosspoint1  = get (raw.xp1, 0.0f);
        cp.crosspoint2  = get (raw.xp2, 1.0f);
        cp.feedback     = get (raw.feedback, 0.0f);
        cp.vibratoDepth = get (raw.vibDepth, 0.0f);
        cp.vibratoRate  = get (raw.vibRate, 5.0f);
        cp.vibratoShape = get (raw.vibShape, 0.0f) > 0.5f ? the89th::VibratoShape::Square
                                                           : the89th::VibratoShape::Sine;

        if (synced)
        {
            const double delaySpan = readout_.shortRange.load() ? full / 10.0 : full;
            cp.delay       = synced01 (static_cast<float> (cp.delay), delaySpan);
            cp.crosspoint1 = synced01 (static_cast<float> (cp.crosspoint1), full);
            cp.crosspoint2 = synced01 (static_cast<float> (cp.crosspoint2), full);
        }
    }

    p.stereo = get (stereo_, 0.0f) > 0.5f ? the89th::StereoMode::Quasi : the89th::StereoMode::True;
    p.range  = get (range_, 0.0f)  > 0.5f ? the89th::DelayRange::Short : the89th::DelayRange::Long;
    p.mix    = get (mix_, 1.0f);

    const int bw = static_cast<int> (get (bandwidth_, 1.0f));
    p.bandwidth = bw == 0 ? the89th::Bandwidth::k5kHz
                : bw == 2 ? the89th::Bandwidth::k20kHz
                          : the89th::Bandwidth::k10kHz;

    const int route = static_cast<int> (get (fbRoute_, 0.0f));
    p.route = route == 1 ? the89th::FeedbackRoute::Cross
            : route == 2 ? the89th::FeedbackRoute::Sum
                         : the89th::FeedbackRoute::Normal;

    p.lowCutHz  = get (lowCut_, 20.0f);
    p.highCutHz = get (highCut_, 20000.0f);
    p.drive     = get (drive_, 0.0f);

    p.scrubDepth = get (scrubDepth_, 0.0f);
    p.scrubRate  = get (scrubRate_, 0.5f);
    p.scrubMode  = get (scrubMode_, 0.0f) > 0.5f ? the89th::ScrubMode::Random : the89th::ScrubMode::Lfo;

    // The KB 2000. The engine turns notes into pitch, latch and envelope.
    auto kb    = [this, &get] (const char* id, float fallback) { return static_cast<double> (get (kbParam (id), fallback)); };
    auto sideOf = [] (double v)
    {
        const int i = static_cast<int> (v);
        return i == 1 ? the89th::Sides::Left : i == 2 ? the89th::Sides::Right
             : i == 3 ? the89th::Sides::Both : the89th::Sides::Off;
    };
    auto& k = p.keys;
    const int chans = static_cast<int> (kb (pid::keys, 0.0f));
    k.channels = chans == 1 ? the89th::KeyChannels::Left  : chans == 2 ? the89th::KeyChannels::Right
               : chans == 3 ? the89th::KeyChannels::Biphonic : the89th::KeyChannels::Off;
    k.play         = kb (pid::kbPlay, 0.0f) > 0.5 ? the89th::KeyPlay::Sustain : the89th::KeyPlay::PushPlay;
    k.root         = static_cast<int> (kb (pid::keysRoot, 60.0f));
    k.trimCents    = kb (pid::kbTrim, 0.0f);
    k.glideSeconds = kb (pid::kbSlope, 0.0f);
    k.addedDelay   = kb (pid::kbAdded, 0.0f);

    k.envelope       = kb (pid::kbEnv, 0.0f) > 0.5;
    k.attackSeconds  = kb (pid::kbAttack, 0.01f);
    k.holdSeconds    = kb (pid::kbHold, 0.5f);
    k.releaseSeconds = kb (pid::kbRelease, 0.3f);

    k.vibrato         = kb (pid::kbVib, 0.0f) > 0.5;
    k.vibRateHz       = kb (pid::kbVibRate, 5.0f);
    k.vibSharpness    = kb (pid::kbVibSharp, 0.0f);
    k.vibDepth        = kb (pid::kbVibDepth, 0.5f);
    k.vibModRate      = kb (pid::kbVibModRate, 0.0f);
    k.vibModSharpness = kb (pid::kbVibModSharp, 0.0f);
    k.vibModDepth     = kb (pid::kbVibModDepth, 0.0f);
    k.vibAttackSeconds  = kb (pid::kbVibAttack, 0.5f);
    k.vibReleaseSeconds = kb (pid::kbVibRelease, 0.5f);

    k.memorySynchro = sideOf (kb (pid::kbSynchro, 0.0f));
    k.attackPoint   = kb (pid::kbAttackPt, 0.0f);
    k.returnPoint   = kb (pid::kbReturnPt, 0.5f);
    k.endPoint      = kb (pid::kbEndPt, 1.0f);
    const double speed = kb (pid::kbSpeed, 1.0f);
    k.speed         = speed < 0.005 ? 0.0 : speed;

    k.reverseSynchro      = sideOf (kb (pid::kbReverse, 0.0f));
    k.noiseGate           = kb (pid::kbGate, 0.0f) > 0.5;
    k.thresholdDb         = kb (pid::kbThresh, -30.0f);
    k.reverseDelaySeconds = kb (pid::kbRevDelay, 0.0f);

    return p;
}

void The89thProcessor::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
        engine_.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())
        engine_.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        engine_.pitchWheel (m.getPitchWheelValue());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine_.allNotesOff();
}

void The89thProcessor::updateReadout()
{
    auto get = [] (std::atomic<float>* a, float fallback) { return a != nullptr ? a->load() : fallback; };

    const int bw = static_cast<int> (get (bandwidth_, 1.0f));
    const auto bandwidth = bw == 0 ? the89th::Bandwidth::k5kHz
                         : bw == 2 ? the89th::Bandwidth::k20kHz
                                   : the89th::Bandwidth::k10kHz;

    the89th::Spec spec;
    spec.channels = get (stereo_, 0.0f) > 0.5f ? 1 : 2;

    readout_.words.store (spec.memoryWordsPerChannel());
    readout_.msPerWord.store (1000.0 / the89th::internalRate (spec, bandwidth));
    readout_.shortRange.store (get (range_, 0.0f) > 0.5f);
    readout_.sync.store (get (sync_, 0.0f) > 0.5f);
    readout_.snap.store (static_cast<int> (get (snap_, 0.0f)));
}

void The89thProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (resetRequested_.exchange (false))
        engine_.reset();

    // Sync follows the host's tempo; without a playhead it runs at 120.
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
                readout_.bpm.store (*bpm);

    updateReadout();

    const int n     = buffer.getNumSamples();
    const int chans = buffer.getNumChannels();
    auto* const* io = buffer.getArrayOfWritePointers();

    // With the keyboard playing, the block runs in pieces split at each MIDI
    // event, so a key lands on the sample it was played rather than the next
    // block. Off, notes still reach the engine so it knows what is held.
    const bool keysOn = kbParam (pid::keys)->load() > 0.5f;
    // Settings first, so a note on the block's first sample meets this
    // block's keyboard rather than the last one's.
    engine_.setParams (readParams());

    int done = 0;
    auto runTo = [&] (int end)
    {
        if (end <= done)
            return;
        engine_.setParams (readParams());
        float* piece[2] = { io[0] + done, chans > 1 ? io[1] + done : io[0] + done };
        engine_.process (piece, chans, end - done);
        done = end;
    };

    for (const auto meta : midi)
    {
        if (keysOn)
            runTo (juce::jlimit (0, n, meta.samplePosition));
        handleMidi (meta.getMessage());
    }
    runTo (n);

    publishTelemetry (buffer);
}

void The89thProcessor::publishTelemetry (const juce::AudioBuffer<float>& buffer) noexcept
{
    const auto& m = engine_.machine();
    const auto& p = m.params();
    const bool quasi = m.quasi();
    const double fs = m.internalSampleRate();

    telemetry_.words.store (m.wordsPerVoice());
    telemetry_.msPerWord.store (static_cast<float> (1000.0 / fs));
    telemetry_.quasi.store (quasi);

    for (int c = 0; c < 2; ++c)
    {
        {
            const auto& kbd = engine_.keyboard();
            auto& tv = telemetry_.voice[static_cast<std::size_t> (c)];
            tv.key.store (kbd.sounding (c));
            tv.synchroPos.store (the89th::onSide (kbd.params().memorySynchro, c) && kbd.drives (c)
                                 ? static_cast<float> (kbd.synchroPosition (c)) : -1.0f);
        }

        const auto ci = static_cast<std::size_t> (c);
        const auto& mem = m.memory (quasi ? 0 : c);
        telemetry_.writePos[ci].store (static_cast<float> (mem.writeIndex()) / static_cast<float> (mem.words()));

        const auto& voice = m.voice (c);
        const auto& trav  = voice.traversal();
        auto& tv = telemetry_.voice[ci];

        const bool onTrav = voice.onTraversal();
        tv.traversal.store (onTrav);
        const auto& cp = c == 0 ? p.left : p.right;
        tv.delayMode.store (cp.mode == the89th::Mode::Delay);
        tv.frozen.store (mem.writeHeld());
        if (onTrav)
        {
            const auto a = trav.primary();
            const auto b = trav.secondary();
            tv.primary.store (static_cast<float> (a.delaySamples));
            tv.gainA.store (a.gain);
            tv.secondary.store (static_cast<float> (b.delaySamples));
            tv.gainB.store (b.gain);
        }
        else
        {
            tv.primary.store (static_cast<float> (voice.delayCurrent()));
            tv.gainA.store (1.0f);
            tv.gainB.store (0.0f);
        }

        tv.regionLo.store (static_cast<float> (trav.regionLo()));
        tv.regionHi.store (static_cast<float> (trav.regionHi()));
        tv.splicing.store (trav.splicing());
        tv.reversed.store (trav.reversed());
        tv.rate.store (static_cast<float> (trav.signedRate()));
        tv.match.store (voice.xing().last().correlation);
        tv.delayMs.store (static_cast<float> (voice.delayTarget() * 1000.0 / fs));

        const int outCh = std::min (c, buffer.getNumChannels() - 1);
        tv.peak.store (buffer.getNumSamples() > 0 ? buffer.getMagnitude (outCh, 0, buffer.getNumSamples()) : 0.0f);
    }
}

juce::AudioProcessorEditor* The89thProcessor::createEditor()
{
    return new The89thEditor (*this);
}

void The89thProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild (presets.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void The89thProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // The preset name and A/B slots ride along in their own child, which the
    // parameter tree must not keep: copyState would hand it back twice.
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto presetTree = tree.getChildWithName (PresetManager::kTreeType);
    if (presetTree.isValid())
        tree.removeChild (presetTree, nullptr);

    // Mode and latch were global once, under the left side's IDs. State saved
    // then has no right-side value: give the right side what the left had.
    for (const auto& [left, right] : { std::pair { pid::channel[0].mode,   pid::channel[1].mode },
                                       std::pair { pid::channel[0].freeze, pid::channel[1].freeze } })
    {
        const auto old = tree.getChildWithProperty ("id", left);
        if (old.isValid() && ! tree.getChildWithProperty ("id", right).isValid())
        {
            auto copy = old.createCopy();
            copy.setProperty ("id", right, nullptr);
            tree.appendChild (copy, nullptr);
        }
    }

    apvts.replaceState (tree);

    // replaceState skips any parameter the tree believes already matches. A
    // switch the host set to 0.36 reads as "off" to the tree but still reports
    // 0.36, so it would never be put back. Set every parameter to the tree's
    // value explicitly.
    for (auto* raw : getParameters())
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (raw))
            if (auto* stored = apvts.getRawParameterValue (param->paramID))
            {
                const float want = param->convertTo0to1 (stored->load());
                if (std::abs (param->getValue() - want) > 1.0e-6f)
                    param->setValueNotifyingHost (want);
            }

    presets.fromTree (presetTree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new The89thProcessor();
}
