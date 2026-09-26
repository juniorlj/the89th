#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"

/** A labelled rotary control bound to one parameter. Shows the parameter's own
    text (ms, st, %, Hz), resets to default on double-click. */
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
          const juce::String& label, bool bipolar = false);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::RangedAudioParameter& param_;
    juce::Slider slider_;
    juce::String label_;
    juce::AudioProcessorValueTreeState::SliderAttachment attach_;
};

/** A row of mutually exclusive segments for a choice parameter: the hardware's
    switches. Labels can be shorter than the parameter's own choice names. */
class SegmentedSwitch final : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    SegmentedSwitch (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                     const juce::String& label, juce::StringArray segmentLabels);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> segmentArea() const;

    juce::RangedAudioParameter& param_;
    juce::String label_;
    juce::StringArray segments_;
    int selected_ = 0;
    juce::ParameterAttachment attach_;
};

/** A large lit button for a bool parameter. Momentary buttons (Init) send true
    and let the processor clear it. */
class LatchButton final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    LatchButton (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                 const juce::String& label, bool momentary = false);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String label_;
    bool on_ = false, momentary_ = false, pressed_ = false;
    juce::ParameterAttachment attach_;
};
