#include "Controls.h"

// ─── Knob ───────────────────────────────────────────────────────────────────

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
            const juce::String& label, bool bipolar)
    : param_ (*state.getParameter (paramId)),
      label_ (label.toUpperCase()),
      attach_ (state, paramId, slider_)
{
    slider_.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider_.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider_.setDoubleClickReturnValue (true, param_.convertFrom0to1 (param_.getDefaultValue()));
    slider_.setMouseDragSensitivity (220);
    slider_.getProperties().set ("bipolar", bipolar);
    slider_.onValueChange = [this] { repaint(); };
    slider_.setTooltip (param_.getName (64) + ". Double-click to reset.");
    addAndMakeVisible (slider_);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int textH = juce::roundToInt (r.getHeight() * 0.34f);
    r.removeFromBottom (textH);
    const int side = std::min (r.getWidth(), r.getHeight());
    slider_.setBounds (r.withSizeKeepingCentre (side, side));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromBottom (r.getHeight() * 0.34f);
    const float h = text.getHeight();

    g.setColour (theme::text);
    g.setFont (theme::font (h * 0.40f, true));
    g.drawFittedText (param_.getCurrentValueAsText(), text.removeFromTop (h * 0.52f).toNearestInt(),
                      juce::Justification::centred, 1);

    g.setColour (theme::textDim);
    g.setFont (theme::font (h * 0.30f).withExtraKerningFactor (0.08f));
    g.drawFittedText (label_, text.toNearestInt(), juce::Justification::centredTop, 1);
}

// ─── SegmentedSwitch ────────────────────────────────────────────────────────

SegmentedSwitch::SegmentedSwitch (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                                  const juce::String& label, juce::StringArray segmentLabels)
    : param_ (*state.getParameter (paramId)),
      label_ (label.toUpperCase()),
      segments_ (std::move (segmentLabels)),
      attach_ (param_, [this] (float v) { selected_ = juce::roundToInt (v); repaint(); })
{
    attach_.sendInitialUpdate();
    setTooltip (param_.getName (64));
}

juce::Rectangle<float> SegmentedSwitch::segmentArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (r.getHeight() * 0.36f);
    return r.reduced (0.5f);
}

void SegmentedSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto labelArea = r.removeFromTop (r.getHeight() * 0.36f);

    g.setColour (theme::textDim);
    g.setFont (theme::font (labelArea.getHeight() * 0.62f).withExtraKerningFactor (0.1f));
    g.drawText (label_, labelArea, juce::Justification::centredLeft);

    const auto area   = segmentArea();
    const float corner = area.getHeight() * 0.22f;

    g.setColour (theme::panelHi);
    g.fillRoundedRectangle (area, corner);
    g.setColour (theme::line);
    g.drawRoundedRectangle (area, corner, 1.0f);

    const int n = segments_.size();
    const float w = area.getWidth() / static_cast<float> (n);
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (area.getX() + w * i, area.getY(), w, area.getHeight());
        const bool on = i == selected_;

        if (on)
        {
            g.setColour (theme::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (seg.reduced (2.0f), corner * 0.8f);
            g.setColour (theme::accent.withAlpha (0.7f));
            g.drawRoundedRectangle (seg.reduced (2.0f), corner * 0.8f, 1.0f);
        }
        else if (i > 0 && i != selected_ + 1)
        {
            g.setColour (theme::line);
            g.drawVerticalLine (juce::roundToInt (seg.getX()), seg.getY() + seg.getHeight() * 0.25f,
                                seg.getBottom() - seg.getHeight() * 0.25f);
        }

        g.setColour (on ? theme::accent : theme::textDim);
        g.setFont (theme::font (area.getHeight() * 0.40f, on));
        g.drawText (segments_[i], seg, juce::Justification::centred);
    }
}

void SegmentedSwitch::mouseDown (const juce::MouseEvent& e)
{
    const auto area = segmentArea();
    if (! area.contains (e.position))
        return;

    const int n = segments_.size();
    const int i = juce::jlimit (0, n - 1, static_cast<int> ((e.position.x - area.getX()) / area.getWidth() * n));
    attach_.setValueAsCompleteGesture (static_cast<float> (i));
}

// ─── LatchButton ────────────────────────────────────────────────────────────

LatchButton::LatchButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                          const juce::String& label, bool momentary)
    : label_ (label.toUpperCase()),
      momentary_ (momentary),
      attach_ (*state.getParameter (paramId), [this] (float v) { on_ = v > 0.5f; repaint(); })
{
    attach_.sendInitialUpdate();
    setTooltip (state.getParameter (paramId)->getName (64));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LatchButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = r.getHeight() * 0.18f;
    const bool lit = on_ || pressed_;

    g.setColour (lit ? theme::accent.withAlpha (momentary_ ? 0.25f : 0.9f) : theme::panelHi);
    g.fillRoundedRectangle (r, corner);
    g.setColour (lit ? theme::accent : theme::lineHi);
    g.drawRoundedRectangle (r, corner, 1.0f);

    g.setColour (lit ? (momentary_ ? theme::accent : theme::bg) : theme::text);
    g.setFont (theme::font (r.getHeight() * (momentary_ ? 0.36f : 0.32f), true).withExtraKerningFactor (0.12f));
    g.drawText (label_, r, juce::Justification::centred);
}

void LatchButton::mouseDown (const juce::MouseEvent&)
{
    pressed_ = true;
    if (momentary_)
        attach_.setValueAsCompleteGesture (1.0f);
    else
        attach_.setValueAsCompleteGesture (on_ ? 0.0f : 1.0f);
    repaint();
}

void LatchButton::mouseUp (const juce::MouseEvent&)
{
    pressed_ = false;
    repaint();
}
