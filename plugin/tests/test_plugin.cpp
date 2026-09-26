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

    for (const char* id : { pid::mode, pid::stereo, pid::range, pid::bandwidth, pid::freeze, pid::mix })
    {
        INFO (id);
        REQUIRE (p.apvts.getParameter (id) != nullptr);
    }
    for (const auto& ch : pid::channel)
        for (const char* id : { ch.delay, ch.pitch, ch.crosspoint1, ch.crosspoint2,
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
    setParam (p, pid::mode, 0.0f);
    setParam (p, pid::channel[0].delay, knobFor ("1/16"));
    runBlock (p);
    const double delayMs = p.engine().machine().voice (0).delayTarget() / 26455.0;
    REQUIRE (delayMs == Approx (0.125).margin (1.0 / 26455.0));
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
