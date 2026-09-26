#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

#include "Theme.h"

/** A skirted knob over a printed scale, with its legend underneath and small
    printed end marks. The value shows in a popup while it turns, and is
    reported through onTouch so the channel display can show it too. */
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
          const juce::String& legend, juce::String minMark = {}, juce::String maxMark = {});

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Called with (legend, value text) whenever the knob is moved. */
    std::function<void (const juce::String&, const juce::String&)> onTouch;

private:
    juce::RangedAudioParameter& param_;
    juce::Slider slider_;
    juce::String legend_, minMark_, maxMark_;
    juce::AudioProcessorValueTreeState::SliderAttachment attach_;
};

/** A row of square push buttons for one choice parameter, each under its own
    LED, the group's title printed above. The hardware's switches. */
class ButtonGroup final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    ButtonGroup (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                 const juce::String& title, juce::StringArray buttonLegends);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> buttonRect (int i) const;

    juce::RangedAudioParameter& param_;
    juce::String title_;
    juce::StringArray legends_;
    int selected_ = 0;
    juce::ParameterAttachment attach_;
};

/** One push button with an LED for a bool parameter. The cream style is the
    panel's single accent, kept for the latch. Momentary buttons (Init) send
    true and let the processor clear it. */
class PushButton final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    enum class Style { Dark, Cream };

    PushButton (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                const juce::String& legend, Style, bool momentary = false);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String legend_;
    Style style_;
    bool on_ = false, momentary_ = false, pressed_ = false;
    juce::ParameterAttachment attach_;
};

namespace draw
{
/** A small LED, lit or dark, with a faint halo when lit. */
void led (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour lit, bool on);

/** A square push-button cap. */
void buttonCap (juce::Graphics&, juce::Rectangle<float>, bool pressed);
} // namespace draw
