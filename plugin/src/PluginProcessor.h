#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <the89th/Engine.hpp>
#include <the89th/Musical.hpp>

#include "Presets.h"
#include "Telemetry.h"

#include <array>
#include <cstring>

/** Thin shell over the core engine: parameters in, blocks through, nothing else.

    The controls follow the hardware: global Stereo, Range and Bandwidth, and
    per channel Mode, Memory Latch, Delay, Pitch, Crosspoint 1 and 2, Feedback
    and Vibrato.

    The modern controls sit on top, each neutral by default: Link, feedback
    routing and tone, Snap and Fine, Sync, Scrub, vibrato shape. The musical
    ones (Link, Snap, Fine, Sync) are resolved here into the plain numbers the
    core takes, so the core keeps one idea of pitch and one of position. */
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
    bool acceptsMidi() const override  { return true; }
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
        std::atomic<bool>   sync       { false };
        std::atomic<double> bpm        { 120.0 };
        std::atomic<int>    snap       { 0 };
    };
    Readout readout_;

public:
    juce::AudioProcessorValueTreeState apvts;

    /** Declared after apvts, which it works on. Saved with the project. */
    PresetManager presets { apvts };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    the89th::EngineParams readParams() const;
    void updateReadout();
    void publishTelemetry (const juce::AudioBuffer<float>&) noexcept;

    /** Init is a momentary control wearing a toggle's clothes, because that is
        all a generic editor offers. Flipping it on hands off to the message
        thread, puts every other parameter back to its default, then flips it
        off again so it reads as a button rather than a state. */
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;

    the89th::Engine   engine_;
    Telemetry         telemetry_;
    std::atomic<bool> resetRequested_ { false };

    struct ChannelRaw
    {
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* freeze = nullptr;
        std::atomic<float>* delay = nullptr;
        std::atomic<float>* pitch = nullptr;
        std::atomic<float>* xp1 = nullptr;
        std::atomic<float>* xp2 = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* vibDepth = nullptr;
        std::atomic<float>* vibRate = nullptr;
        std::atomic<float>* fine = nullptr;
        std::atomic<float>* vibShape = nullptr;
    };
    std::array<ChannelRaw, 2> ch_ {};

    std::atomic<float>* stereo_    = nullptr;
    std::atomic<float>* range_     = nullptr;
    std::atomic<float>* bandwidth_ = nullptr;
    std::atomic<float>* mix_       = nullptr;

    std::atomic<float>* link_       = nullptr;
    std::atomic<float>* fbRoute_    = nullptr;
    std::atomic<float>* lowCut_     = nullptr;
    std::atomic<float>* highCut_    = nullptr;
    std::atomic<float>* drive_      = nullptr;
    std::atomic<float>* snap_       = nullptr;
    std::atomic<float>* sync_       = nullptr;
    std::atomic<float>* scrubDepth_ = nullptr;
    std::atomic<float>* scrubRate_  = nullptr;
    std::atomic<float>* scrubMode_  = nullptr;
    /** The keyboard's controls, by ID. Filled once in the constructor; looked
        up by a scan, which allocates nothing on the audio thread. */
    std::array<std::pair<const char*, std::atomic<float>*>, 28> kb_ {};

    std::atomic<float>* kbParam (const char* id) const noexcept
    {
        for (const auto& [name, value] : kb_)
            if (name != nullptr && std::strcmp (name, id) == 0)
                return value;
        return nullptr;
    }

    /** MIDI to the engine's keyboard. Audio thread only. */
    void handleMidi (const juce::MidiMessage&) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thProcessor)
};
