#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class The89thProcessor;

/** Generic parameter list plus a build stamp, so a stale host load is obvious. */
class The89thEditor final : public juce::AudioProcessorEditor
{
public:
    explicit The89thEditor (The89thProcessor&);
    ~The89thEditor() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Label stamp_;
    juce::GenericAudioProcessorEditor params_;

    static constexpr int kStampH = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thEditor)
};
