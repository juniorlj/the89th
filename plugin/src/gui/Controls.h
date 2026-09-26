#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

#include "Theme.h"

/** An encoder with its LED ring, end marks, name, and value always shown in
    orange underneath. Moves are reported through onTouch so the channel
    display can echo them. */
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

/** A segmented selector for one choice parameter: flat outlined segments,
    the active one filled orange, a status dot above each, title above. */
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

/** One button for a bool parameter. Accent (the latch) is outlined orange
    and fills solid when on; Plain is a hairline button with its legend
    underneath; Chip is a compact toggle, a dot and the legend in one outline,
    that lights orange when on. Momentary buttons (Init) send true and let the
    processor clear it. */
class PushButton final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    enum class Style { Plain, Accent, Chip };

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

/** A chip like PushButton's, for editor state rather than a parameter:
    which page is showing, say. */
class ChipToggle final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit ChipToggle (const juce::String& legend) : legend_ (legend.toUpperCase())
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setOn (bool on) { on_ = on; repaint(); }
    bool isOn() const noexcept { return on_; }

    std::function<void (bool)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String legend_;
    bool on_ = false;
};

namespace draw
{
/** A small status dot: orange when on, dark when off. */
void dot (juce::Graphics&, juce::Point<float> centre, float radius, bool on);
} // namespace draw
