#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/ParameterIDs.h"
#include "../src/PluginProcessor.h"

using Catch::Approx;

namespace
{
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
            REQUIRE (p.engine().latencySamples() == static_cast<double> (latency));
        }
}

TEST_CASE ("the editor opens large, resizes, and keeps its proportions", "[plugin][gui]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    REQUIRE (ed->getWidth()  == 1100);
    REQUIRE (ed->getHeight() == 680);
    REQUIRE (ed->isResizable());

    auto* c = ed->getConstrainer();
    REQUIRE (c != nullptr);
    REQUIRE (c->getMinimumWidth() == 880);
    REQUIRE (c->getMaximumWidth() == 1760);

    // Draw at both ends of the range; a layout that breaks at one size tends to
    // assert or divide by zero when painted.
    for (int w : { 880, 1100, 1760 })
    {
        ed->setSize (w, w * 680 / 1100);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        REQUIRE (img.getWidth() == w);
    }
}
