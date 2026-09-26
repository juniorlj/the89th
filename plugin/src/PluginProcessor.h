#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <the89th/Engine.hpp>

#include "Telemetry.h"

#include <array>

/** Thin shell over the core engine: parameters in, blocks through, nothing else.

    The controls follow the hardware: global Mode, Stereo, Range, Bandwidth and
    Freeze (the latch acts on both channels), and per channel Delay, Pitch,
    Crosspoint 1 and 2, Feedback and Vibrato. */
class The89thProcessor final : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::AsyncUpdater
{
public:
    The89thProcessor();
    ~The89thProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "THE89TH"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const the89th::Engine& engine() const noexcept { return engine_; }

    /** Written by the audio thread once per block; read by the editor. */
    const Telemetry& telemetry() const noexcept { return telemetry_; }

private:
    /** What the millisecond readouts need to know about the machine's current
        layout. Declared before apvts: its text functions read these. */
    struct Readout
    {
        std::atomic<double> msPerWord  { 1000.0 / 26455.0 };
        std::atomic<int>    words      { 8192 };
        std::atomic<bool>   shortRange { false };
    };
    Readout readout_;

public:
    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    the89th::EngineParams readParams() const;
    void updateReadout();
    void publishTelemetry (const juce::AudioBuffer<float>&) noexcept;

    /** Init is a momentary control wearing a toggle's clothes, because that is
        all a generic editor offers. Flipping it on hands off to the message
        thread, puts every other parameter back to its default, then flips it
        off again so it reads as a button rather than a state.

        The same handoff reports a latency change: bandwidth and stereo layout
        move the converter clock, and with it the resampler's latency, but a
        host must hear about that from the message thread. */
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;

    the89th::Engine   engine_;
    Telemetry         telemetry_;
    std::atomic<bool> resetRequested_ { false };
    std::atomic<bool> initRequested_  { false };

    /** Audio thread only: what the host was last told, to spot a change. */
    int              latencyReported_ = 0;
    std::atomic<int> latencyPending_ { -1 };

    struct ChannelRaw
    {
        std::atomic<float>* delay = nullptr;
        std::atomic<float>* pitch = nullptr;
        std::atomic<float>* xp1 = nullptr;
        std::atomic<float>* xp2 = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* vibDepth = nullptr;
        std::atomic<float>* vibRate = nullptr;
    };
    std::array<ChannelRaw, 2> ch_ {};

    std::atomic<float>* mode_      = nullptr;
    std::atomic<float>* stereo_    = nullptr;
    std::atomic<float>* range_     = nullptr;
    std::atomic<float>* bandwidth_ = nullptr;
    std::atomic<float>* freeze_    = nullptr;
    std::atomic<float>* mix_       = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thProcessor)
};
