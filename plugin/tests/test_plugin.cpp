#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <functional>

#include "../src/ParameterIDs.h"
#include "../src/PluginProcessor.h"

using Catch::Approx;

namespace
{
/** Exact equality, for values that must come through untouched. Spelt this
    way so it doesn't trip -Wfloat-equal. */
bool same (double a, double b) { return std::equal_to<double> {} (a, b); }

void pumpMessageThread (int ms = 200)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

/** Every parameter Init is responsible for: all of them but Init and Build. */
std::vector<juce::RangedAudioParameter*> resettable (The89thProcessor& p)
{
    std::vector<juce::RangedAudioParameter*> out;
    for (auto* raw : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (raw))
            if (r->paramID != pid::init && r->paramID != pid::build)
                out.push_back (r);
    return out;
}
} // namespace

TEST_CASE ("the panel matches the hardware's controls", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;

    for (const char* id : { pid::stereo, pid::range, pid::bandwidth, pid::mix })
    {
        INFO (id);
        REQUIRE (p.apvts.getParameter (id) != nullptr);
    }
    for (const auto& ch : pid::channel)
        for (const char* id : { ch.mode, ch.freeze, ch.delay, ch.pitch, ch.crosspoint1, ch.crosspoint2,
                                ch.feedback, ch.vibratoDepth, ch.vibratoRate })
        {
            INFO (id);
            REQUIRE (p.apvts.getParameter (id) != nullptr);
        }
}

TEST_CASE ("Init puts every parameter back to its default", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    const auto params = resettable (p);
    REQUIRE (params.size() >= 20);

    // Move everything somewhere that is not its default.
    for (auto* param : params)
        param->setValueNotifyingHost (param->getDefaultValue() < 0.5f ? 0.9f : 0.1f);

    for (auto* param : params)
    {
        INFO (param->paramID);
        REQUIRE (param->getValue() != Approx (param->getDefaultValue()));
    }

    p.apvts.getParameter (pid::init)->setValueNotifyingHost (1.0f);
    pumpMessageThread();

    for (auto* param : params)
    {
        INFO (param->paramID);
        REQUIRE (param->getValue() == Approx (param->getDefaultValue()));
    }

    // Init is momentary: it has to clear itself or it reads as a latched state.
    REQUIRE (p.apvts.getParameter (pid::init)->getValue() == Approx (0.0f));
}

TEST_CASE ("crosspoints read in milliseconds that follow the clock", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    auto* xp2 = p.apvts.getParameter (pid::crosspoint2);

    // True stereo at 10 kHz: 8189 words at 26455 Hz is 309.5 ms.
    REQUIRE (xp2->getText (1.0f, 32) == "310 ms");

    // 5 kHz halves the clock, so the same position is twice as long.
    p.apvts.getParameter (pid::bandwidth)->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> b (2, 64);
    juce::MidiBuffer m;
    p.prepareToPlay (48000.0, 64);
    b.clear();
    p.processBlock (b, m);
    REQUIRE (xp2->getText (1.0f, 32) == "619 ms");
}

TEST_CASE ("Init clears the delay memory too", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    // Fill memory with a loud tone for well past the delay length.
    for (int block = 0; block < 120; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.9f * std::sin (0.05f * static_cast<float> (block * 512 + i)));

        p.processBlock (buffer, midi);
    }

    buffer.clear();
    p.processBlock (buffer, midi);
    REQUIRE (buffer.getMagnitude (0, 512) > 0.05f);   // memory is holding audio

    p.apvts.getParameter (pid::init)->setValueNotifyingHost (1.0f);
    pumpMessageThread();

    // First block after Init consumes the reset request.
    buffer.clear();
    p.processBlock (buffer, midi);
    buffer.clear();
    p.processBlock (buffer, midi);

    REQUIRE (buffer.getMagnitude (0, 512) == Approx (0.0f).margin (1.0e-6f));
}

TEST_CASE ("the plugin passes audio and stays finite", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    p.apvts.getParameter (pid::feedback)->setValueNotifyingHost (0.8f);
    p.apvts.getParameter (pid::pitch)->setValueNotifyingHost (0.8f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int block = 0; block < 400; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.9f * std::sin (0.03f * static_cast<float> (block * 512 + i)));

        p.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    }
}

TEST_CASE ("restoring a state restores every parameter including switches", "[plugin]")
{
    // A host can hand a switch any value between 0 and 1. JUCE's parameter
    // tree records the switch as already matching the state, so a plain
    // replaceState leaves it where the host put it. Found by pluginval.
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;

    juce::MemoryBlock saved;
    p.getStateInformation (saved);

    for (auto* param : resettable (p))
    {
        const float original = param->getValue();
        param->setValue (original < 0.5f ? 0.36f : 0.64f);

        p.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

        INFO (param->paramID);
        REQUIRE (param->getValue() == Approx (original).margin (1.0e-6));
    }
}

TEST_CASE ("latency stays put across every clock setting", "[plugin]")
{
    // A host has to restart a plugin to take a new latency, and a restart
    // empties the memory. So a bandwidth or layout switch must not change it,
    // or the octave jump on a bandwidth switch becomes a gap.
    juce::ScopedJuceInitialiser_GUI gui;

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);
    const int latency = p.getLatencySamples();
    REQUIRE (latency > 0);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (float stereo : { 0.0f, 1.0f })
        for (float bw : { 0.0f, 0.5f, 1.0f })
        {
            p.apvts.getParameter (pid::stereo)->setValueNotifyingHost (stereo);
            p.apvts.getParameter (pid::bandwidth)->setValueNotifyingHost (bw);
            buffer.clear();
            p.processBlock (buffer, midi);
            pumpMessageThread (20);

            INFO ("stereo " << stereo << ", bandwidth " << bw);
            REQUIRE (p.getLatencySamples() == latency);
            REQUIRE (same (p.engine().latencySamples(), static_cast<double> (latency)));
        }
}

namespace
{
void setParam (The89thProcessor& p, const char* id, float plainValue)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

void runBlock (The89thProcessor& p)
{
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    buffer.clear();
    p.processBlock (buffer, midi);
}
} // namespace

TEST_CASE ("every modern control starts neutral, so a fresh instance is the machine", "[plugin][modern]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);
    runBlock (p);

    const auto& e = p.engine().params();
    const the89th::EngineParams hw;
    REQUIRE (e.route == hw.route);
    REQUIRE (same (e.lowCutHz, hw.lowCutHz));
    REQUIRE (same (e.highCutHz, hw.highCutHz));
    REQUIRE (same (e.drive, hw.drive));
    REQUIRE (same (e.scrubDepth, hw.scrubDepth));
    REQUIRE (same (e.left.pitchRatio, 1.0));
    REQUIRE (e.left.vibratoShape == the89th::VibratoShape::Sine);
    REQUIRE (e.right.vibratoShape == the89th::VibratoShape::Sine);
}

TEST_CASE ("link makes channel 2 follow channel 1", "[plugin][modern]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    setParam (p, pid::channel[0].pitch, 1.5f);
    setParam (p, pid::channel[1].pitch, 0.5f);
    setParam (p, pid::channel[0].feedback, 0.6f);
    runBlock (p);
    REQUIRE (p.engine().params().right.pitchRatio == Approx (0.5));

    setParam (p, pid::link, 1.0f);
    runBlock (p);
    const auto& e = p.engine().params();
    REQUIRE (same (e.right.pitchRatio, e.left.pitchRatio));
    REQUIRE (same (e.right.feedback, e.left.feedback));
    REQUIRE (same (e.right.crosspoint2, e.left.crosspoint2));
}

TEST_CASE ("snap and fine reach the engine as one ratio", "[plugin][modern]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    setParam (p, pid::channel[0].pitch, 1.41f);           // 5.94 semitones
    setParam (p, pid::snap, 1.0f);                         // chromatic
    runBlock (p);
    REQUIRE (p.engine().params().left.pitchRatio == Approx (std::exp2 (6.0 / 12.0)));
    REQUIRE (p.apvts.getParameter (pid::channel[0].pitch)->getCurrentValueAsText().contains ("6.00 st"));

    setParam (p, pid::channel[0].fine, 50.0f);
    runBlock (p);
    REQUIRE (p.engine().params().left.pitchRatio == Approx (std::exp2 (6.5 / 12.0)));
}

TEST_CASE ("sync puts the crosspoints and delay on note values", "[plugin][modern]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);   // no playhead, so 120 BPM

    namespace mus = the89th::musical;
    auto knobFor = [] (const char* note)
    {
        for (int s = 1; s < mus::kSyncSteps; ++s)
            if (juce::String (mus::syncName (s)) == note)
                return static_cast<float> (s) / (mus::kSyncSteps - 1);
        return 0.0f;
    };

    setParam (p, pid::sync, 1.0f);
    setParam (p, pid::channel[0].crosspoint2, knobFor ("1/8"));
    setParam (p, pid::channel[0].crosspoint1, 0.0f);
    runBlock (p);

    // True stereo at 10 kHz: 26455 words a second. A 1/8 at 120 BPM is 250 ms.
    const auto& t = p.engine().machine().voice (0).traversal();
    REQUIRE (t.regionHi() / 26455.0 == Approx (0.250).margin (1.0 / 26455.0));
    REQUIRE (p.apvts.getParameter (pid::channel[0].crosspoint2)->getCurrentValueAsText() == "1/8");

    // A 1/2 is a second: longer than the 310 ms memory, so it clamps and says so.
    setParam (p, pid::channel[0].crosspoint2, knobFor ("1/2"));
    runBlock (p);
    REQUIRE (same (p.engine().params().left.crosspoint2, 1.0));
    REQUIRE (p.apvts.getParameter (pid::channel[0].crosspoint2)->getCurrentValueAsText() == "1/2 > MAX");

    // Delay mode follows too.
    setParam (p, pid::channel[0].mode, 0.0f);
    setParam (p, pid::channel[0].delay, knobFor ("1/16"));
    runBlock (p);
    const double delayMs = p.engine().machine().voice (0).delayTarget() / 26455.0;
    REQUIRE (delayMs == Approx (0.125).margin (1.0 / 26455.0));
}

// ─── Per-channel mode and latch ─────────────────────────────────────────────

TEST_CASE ("each side has its own mode and latch", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    setParam (p, pid::channel[1].mode, 0.0f);     // right: delay
    setParam (p, pid::channel[0].freeze, 1.0f);   // left: latched
    runBlock (p);

    const auto& e = p.engine().params();
    REQUIRE (e.left.mode  == the89th::Mode::Pitch);
    REQUIRE (e.right.mode == the89th::Mode::Delay);
    REQUIRE (e.left.freeze);
    REQUIRE_FALSE (e.right.freeze);
    REQUIRE (p.engine().machine().memory (0).writeHeld());
    REQUIRE_FALSE (p.engine().machine().memory (1).writeHeld());
}

TEST_CASE ("state from when mode and latch were global sets both sides", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor before;
    setParam (before, pid::mode, 0.0f);
    setParam (before, pid::freeze, 1.0f);

    juce::MemoryBlock block;
    before.getStateInformation (block);

    // Strip the right side's entries, which older state doesn't have.
    auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), static_cast<int> (block.getSize()));
    REQUIRE (xml != nullptr);
    for (const char* id : { pid::channel[1].mode, pid::channel[1].freeze })
        if (auto* child = xml->getChildByAttribute ("id", id))
            xml->removeChildElement (child, true);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary (*xml, old);

    The89thProcessor after;
    after.setStateInformation (old.getData(), static_cast<int> (old.getSize()));
    auto value = [&after] (const char* id)
    {
        auto* param = after.apvts.getParameter (id);
        return param->convertFrom0to1 (param->getValue());
    };
    REQUIRE (value (pid::channel[1].mode) < 0.5f);
    REQUIRE (value (pid::channel[1].freeze) > 0.5f);
}

// ─── Presets and A/B ────────────────────────────────────────────────────────

namespace
{
float plain (The89thProcessor& p, const char* id)
{
    auto* param = p.apvts.getParameter (id);
    return param->convertFrom0to1 (param->getValue());
}

struct TempFolder
{
    juce::File dir = juce::File::createTempFile ("the89th-presets");
    TempFolder()  { dir.createDirectory(); }
    ~TempFolder() { dir.deleteRecursively(); }
};
} // namespace

TEST_CASE ("factory presets only name real parameters, inside their ranges", "[plugin][presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;

    REQUIRE (PresetManager::factory().size() >= 10);
    REQUIRE (PresetManager::factory().front().first == "Init");
    REQUIRE (PresetManager::factory().front().second.empty());

    for (const auto& [name, values] : PresetManager::factory())
        for (const auto& [id, v] : values)
        {
            INFO (name << ": " << id);
            auto* param = p.apvts.getParameter (id);
            REQUIRE (param != nullptr);
            const auto range = param->getNormalisableRange().getRange();
            REQUIRE (v >= range.getStart());
            REQUIRE (v <= range.getEnd());
        }
}

TEST_CASE ("loading a preset sets what it lists and resets the rest", "[plugin][presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    TempFolder tmp;
    PresetManager presets (p.apvts, tmp.dir);

    setParam (p, pid::mix, 0.2f);
    setParam (p, pid::drive, 0.7f);

    int octaveUp = -1;
    for (int i = 0; i < static_cast<int> (presets.entries().size()); ++i)
        if (presets.entries()[static_cast<std::size_t> (i)].name == "Octave Up")
            octaveUp = i;
    REQUIRE (presets.load (octaveUp));

    REQUIRE (plain (p, pid::channel[0].pitch) == Approx (2.0f));
    REQUIRE (plain (p, pid::mix) == Approx (1.0f));     // default again
    REQUIRE (plain (p, pid::drive) == Approx (0.0f));
    REQUIRE (presets.currentName() == "Octave Up");
    REQUIRE_FALSE (presets.modified());

    setParam (p, pid::mix, 0.5f);
    REQUIRE (presets.modified());
}

TEST_CASE ("a saved preset comes back exactly", "[plugin][presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    TempFolder tmp;
    PresetManager presets (p.apvts, tmp.dir);

    setParam (p, pid::channel[0].pitch, 1.37f);
    setParam (p, pid::channel[1].fine, -23.0f);
    setParam (p, pid::fbRoute, 2.0f);
    setParam (p, pid::highCut, 3300.0f);
    const auto saved = presets.capture();

    REQUIRE (presets.save ("My Sound"));
    REQUIRE (tmp.dir.getChildFile ("My Sound.the89th").existsAsFile());
    REQUIRE_FALSE (presets.save ("Octave Up"));   // factory names are taken
    REQUIRE_FALSE (presets.save ("   "));

    presets.load (0);   // Init
    REQUIRE (plain (p, pid::fbRoute) == Approx (0.0f));

    const auto& list = presets.entries();
    const auto it = std::find_if (list.begin(), list.end(), [] (const auto& e) { return e.name == "My Sound"; });
    REQUIRE (it != list.end());
    REQUIRE_FALSE (it->factory);
    REQUIRE (presets.load (static_cast<int> (it - list.begin())));

    for (const auto& [id, v] : saved)
    {
        INFO (id);
        REQUIRE (plain (p, id.toRawUTF8()) == Approx (v).margin (1.0e-5));
    }

    REQUIRE (presets.remove (static_cast<int> (it - list.begin())));
    REQUIRE_FALSE (tmp.dir.getChildFile ("My Sound.the89th").exists());
    REQUIRE_FALSE (presets.remove (0));   // factory presets stay
}

TEST_CASE ("A/B keeps two settings and swaps between them", "[plugin][presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    TempFolder tmp;
    PresetManager presets (p.apvts, tmp.dir);

    setParam (p, pid::channel[0].pitch, 1.5f);
    REQUIRE (presets.activeSlot() == 0);

    presets.selectSlot (1);                      // B starts as a copy of A
    REQUIRE (presets.activeSlot() == 1);
    REQUIRE (plain (p, pid::channel[0].pitch) == Approx (1.5f));

    setParam (p, pid::channel[0].pitch, 0.5f);
    presets.selectSlot (0);
    REQUIRE (plain (p, pid::channel[0].pitch) == Approx (1.5f));
    presets.selectSlot (1);
    REQUIRE (plain (p, pid::channel[0].pitch) == Approx (0.5f));

    presets.copyToOtherSlot();                   // B onto A
    presets.selectSlot (0);
    REQUIRE (plain (p, pid::channel[0].pitch) == Approx (0.5f));
}

TEST_CASE ("the project keeps the preset name and both A/B slots", "[plugin][presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::MemoryBlock saved;
    {
        The89thProcessor p;
        p.presets.load (1);
        setParam (p, pid::channel[0].pitch, 1.25f);
        p.presets.selectSlot (1);
        setParam (p, pid::channel[0].pitch, 0.75f);
        p.getStateInformation (saved);
    }

    The89thProcessor q;
    q.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    REQUIRE (q.presets.currentName() == PresetManager::factory()[1].first);
    REQUIRE (q.presets.activeSlot() == 1);
    REQUIRE (plain (q, pid::channel[0].pitch) == Approx (0.75f));

    q.presets.selectSlot (0);
    REQUIRE (plain (q, pid::channel[0].pitch) == Approx (1.25f));

    // Saving again straight after a restore gives back the same bytes.
    The89thProcessor r;
    r.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    juce::MemoryBlock again;
    r.getStateInformation (again);
    REQUIRE (again == saved);
}

TEST_CASE ("the editor opens large, resizes, and keeps its proportions", "[plugin][gui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    REQUIRE (ed->getWidth()  == 1100);
    REQUIRE (ed->getHeight() == 800);
    REQUIRE (ed->isResizable());

    auto* c = ed->getConstrainer();
    REQUIRE (c != nullptr);
    REQUIRE (c->getMinimumWidth() == 880);
    REQUIRE (c->getMaximumWidth() == 1760);

    // Draw at both ends of the range; a layout that breaks at one size tends to
    // assert or divide by zero when painted.
    for (int w : { 880, 1100, 1760 })
    {
        ed->setSize (w, w * 800 / 1100);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        REQUIRE (img.getWidth() == w);
    }
}

// ─── Keyboard layer ─────────────────────────────────────────────────────────

namespace
{
void setChoice (The89thProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

/** Runs one 512-sample block of a 330 Hz tone (or silence) with the given MIDI. */
void block (The89thProcessor& p, juce::AudioBuffer<float>& b, juce::MidiBuffer& midi, long long& t, bool tone)
{
    for (int i = 0; i < b.getNumSamples(); ++i, ++t)
    {
        const float x = tone ? 0.5f * static_cast<float> (std::sin (2.0 * M_PI * 330.0 * static_cast<double> (t) / 48000.0)) : 0.0f;
        b.setSample (0, i, x);
        b.setSample (1, i, x);
    }
    p.processBlock (b, midi);
    midi.clear();
}

float peak (const juce::AudioBuffer<float>& b, int from = 0)
{
    return b.getMagnitude (0, from, b.getNumSamples() - from);
}

/** Frequency from positive-going zero crossings over a run of blocks. */
double measureHz (The89thProcessor& p, long long& t, int blocks)
{
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer none;
    int crossings = 0;
    float prev = 0.0f;
    for (int k = 0; k < blocks; ++k)
    {
        block (p, b, none, t, true);
        for (int i = 0; i < 512; ++i)
        {
            const float y = b.getSample (0, i);
            if (prev <= 0.0f && y > 0.0f)
                ++crossings;
            prev = y;
        }
    }
    return crossings * 48000.0 / (blocks * 512.0);
}
} // namespace

TEST_CASE ("with Keys off, MIDI changes nothing", "[plugin][keys]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    auto render = [] (bool sendMidi)
    {
        The89thProcessor p;
        p.prepareToPlay (48000.0, 512);
        setChoice (p, pid::crosspoint2, 0.2f);
        juce::AudioBuffer<float> b (2, 512), out (2, 512 * 40);
        juce::MidiBuffer midi;
        long long t = 0;
        for (int k = 0; k < 40; ++k)
        {
            if (sendMidi && k == 5)  midi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 100);
            if (sendMidi && k == 20) midi.addEvent (juce::MidiMessage::noteOff (1, 72), 50);
            block (p, b, midi, t, true);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, k * 512, b, ch, 0, 512);
        }
        return out;
    };

    const auto a = render (false), b = render (true);
    int mismatches = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (! std::equal_to<float> {} (a.getSample (ch, i), b.getSample (ch, i)))
                ++mismatches;
    REQUIRE (mismatches == 0);
}

TEST_CASE ("keys: silent until a key, the key sets the pitch, release latches and mutes", "[plugin][keys]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);
    setChoice (p, pid::keys, 1.0f);             // L+R
    setChoice (p, pid::crosspoint2, 0.15f);     // a short loop, ~46 ms

    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer midi;
    long long t = 0;

    // No key held: latched and muted, whatever comes in.
    for (int k = 0; k < 20; ++k)
        block (p, b, midi, t, true);
    REQUIRE (peak (b) < 1e-5f);

    // Root plus an octave: the tone comes back an octave up.
    midi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 0);
    block (p, b, midi, t, true);
    for (int k = 0; k < 20; ++k)
        block (p, b, midi, t, true);
    REQUIRE (peak (b) > 0.1f);
    REQUIRE (measureHz (p, t, 40) == Approx (660.0).epsilon (0.03));

    // Release: muted within the fade, and the memory is latched.
    midi.addEvent (juce::MidiMessage::noteOff (1, 72), 0);
    block (p, b, midi, t, true);
    block (p, b, midi, t, true);
    REQUIRE (peak (b) < 1e-5f);

    // With the input now silent, a new key plays back what was latched.
    for (int k = 0; k < 10; ++k)
        block (p, b, midi, t, false);
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    block (p, b, midi, t, false);
    block (p, b, midi, t, false);
    REQUIRE (peak (b) > 0.1f);
}

TEST_CASE ("keys: a note lands on its own sample, not the next block", "[plugin][keys]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);
    setChoice (p, pid::keys, 1.0f);
    setChoice (p, pid::crosspoint2, 0.05f);

    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer midi;
    long long t = 0;

    // Fill the memory with the gate shut, then open it mid-block.
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    block (p, b, midi, t, true);
    for (int k = 0; k < 10; ++k)
        block (p, b, midi, t, true);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    block (p, b, midi, t, true);
    block (p, b, midi, t, true);

    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 300);
    block (p, b, midi, t, true);

    REQUIRE (b.getMagnitude (0, 0, 300) < 1e-5f);    // still shut before the note
    REQUIRE (b.getMagnitude (0, 300, 212) > 0.05f);  // opening from sample 300
}

TEST_CASE ("keys: root follows its parameter, and routing picks the channel", "[plugin][keys]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);
    setChoice (p, pid::keys, 2.0f);             // Left only
    setChoice (p, pid::keysRoot, 48.0f);        // C2 is unity
    setChoice (p, pid::channel[0].crosspoint2, 0.15f);
    setChoice (p, pid::channel[1].crosspoint2, 0.15f);

    REQUIRE (p.apvts.getParameter (pid::keysRoot)->getCurrentValueAsText() == "C2");

    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer midi;
    long long t = 0;

    // Right isn't driven, so it plays on with no key; left is muted.
    for (int k = 0; k < 20; ++k)
        block (p, b, midi, t, true);
    REQUIRE (b.getMagnitude (0, 0, 512) < 1e-5f);
    REQUIRE (b.getMagnitude (1, 0, 512) > 0.1f);

    // Key 48 is the new root: unity, so left comes back at the input's pitch.
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
    block (p, b, midi, t, true);
    REQUIRE (measureHz (p, t, 40) == Approx (330.0).epsilon (0.03));
}
