#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <the89th/Engine.hpp>

/** Thin shell over the core engine: parameters in, blocks through, nothing else.

    One parameter set drives both channels. The hardware has independent
    per-channel controls and EngineParams already carries separate left and
    right structs, so splitting them later is a layout change here and no DSP
    change at all. */
class The89thProcessor final : public juce::AudioProcessor
{
public:
    The89thProcessor();
    ~The89thProcessor() override = default;

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

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    the89th::EngineParams readParams() const;

    template <typename T>
    T* raw (const char* id) const
    {
        return dynamic_cast<T*> (apvts.getParameter (id));
    }

    the89th::Engine engine_;

    std::atomic<float>* pitch_    = nullptr;
    std::atomic<float>* xp1_      = nullptr;
    std::atomic<float>* xp2_      = nullptr;
    std::atomic<float>* feedback_ = nullptr;
    std::atomic<float>* mix_      = nullptr;
    std::atomic<float>* bandwidth_ = nullptr;
    std::atomic<float>* freeze_   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thProcessor)
};
