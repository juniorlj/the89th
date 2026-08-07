#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/ParameterIDs.h"
#include "../src/PluginProcessor.h"

using Catch::Approx;

namespace
{
const char* kMusical[] = { pid::pitch, pid::crosspoint1, pid::crosspoint2,
                           pid::feedback, pid::mix, pid::bandwidth, pid::freeze };

void pumpMessageThread (int ms = 200)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}
} // namespace

TEST_CASE ("Init puts every parameter back to its default", "[plugin]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    The89thProcessor p;
    p.prepareToPlay (48000.0, 512);

    auto param = [&] (const char* id) { return p.apvts.getParameter (id); };

    // Move everything somewhere that is not its default.
    param (pid::pitch)->setValueNotifyingHost (0.9f);
    param (pid::crosspoint1)->setValueNotifyingHost (0.7f);
    param (pid::crosspoint2)->setValueNotifyingHost (0.2f);
    param (pid::feedback)->setValueNotifyingHost (0.6f);
    param (pid::mix)->setValueNotifyingHost (0.3f);
    param (pid::bandwidth)->setValueNotifyingHost (0.0f);
    param (pid::freeze)->setValueNotifyingHost (1.0f);

    for (auto* id : kMusical)
    {
        INFO (id);
        REQUIRE (param (id)->getValue() != Approx (param (id)->getDefaultValue()));
    }

    param (pid::init)->setValueNotifyingHost (1.0f);
    pumpMessageThread();

    for (auto* id : kMusical)
    {
        INFO (id);
        REQUIRE (param (id)->getValue() == Approx (param (id)->getDefaultValue()));
    }

    // Init is momentary: it has to clear itself or it reads as a latched state.
    REQUIRE (param (pid::init)->getValue() == Approx (0.0f));
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
