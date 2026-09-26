#include "Controls.h"

namespace draw
{
void dot (juce::Graphics& g, juce::Point<float> c, float r, bool on)
{
    g.setColour (on ? theme::orange : theme::dotOff);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
}
} // namespace draw

// ─── Knob ───────────────────────────────────────────────────────────────────

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
            const juce::String& legend, juce::String minMark, juce::String maxMark)
    : param_ (*state.getParameter (paramId)),
      legend_ (legend.toUpperCase()),
      minMark_ (std::move (minMark)),
      maxMark_ (std::move (maxMark)),
      attach_ (state, paramId, slider_)
{
    slider_.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider_.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider_.setDoubleClickReturnValue (true, param_.convertFrom0to1 (param_.getDefaultValue()));
    slider_.setMouseDragSensitivity (240);
    slider_.getProperties().set ("bipolar", paramId.startsWith ("pitch") || paramId.startsWith ("fine"));
    slider_.setTooltip (param_.getName (64) + ". Double-click to reset.");

    auto report = [this]
    {
        if (onTouch)
            onTouch (legend_, param_.getCurrentValueAsText());
    };
    slider_.onDragStart   = report;
    slider_.onValueChange = [this, report]
    {
        repaint();
        if (slider_.isMouseButtonDown())
            report();
    };

    addAndMakeVisible (slider_);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (juce::roundToInt (r.getHeight() * 0.36f));
    const int side = std::min (r.getWidth(), r.getHeight());
    slider_.setBounds (r.withSizeKeepingCentre (side, side));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromBottom (r.getHeight() * 0.36f);
    const auto knob = slider_.getBounds().toFloat();

    // Type follows the width, which every encoder shares, so all of them
    // print at the same size whatever their height.
    const float wUnit = r.getWidth() / 100.0f;

    // End marks under the ends of the ring, inside the component.
    if (minMark_.isNotEmpty() || maxMark_.isNotEmpty())
    {
        const auto full = getLocalBounds().toFloat();
        const float mh = 12.0f * wUnit;
        const float y  = knob.getBottom() - mh;
        g.setColour (theme::textDim);
        g.setFont (theme::mono (9.0f * wUnit));
        g.drawText (minMark_, juce::Rectangle<float> (full.getX(), y, full.getWidth() * 0.4f, mh),
                    juce::Justification::centredLeft);
        g.drawText (maxMark_, juce::Rectangle<float> (full.getRight() - full.getWidth() * 0.4f, y, full.getWidth() * 0.4f, mh),
                    juce::Justification::centredRight);
    }

    const auto nameArea  = text.removeFromTop (17.0f * wUnit);
    const auto valueArea = text.removeFromTop (17.0f * wUnit);

    g.setColour (theme::text);
    g.setFont (theme::mono (11.5f * wUnit));
    g.drawFittedText (legend_, nameArea.toNearestInt(), juce::Justification::centred, 1);

    g.setColour (theme::orange);
    g.setFont (theme::mono (12.0f * wUnit, true));
    g.drawFittedText (param_.getCurrentValueAsText(), valueArea.toNearestInt(), juce::Justification::centred, 1);
}

// ─── ButtonGroup ────────────────────────────────────────────────────────────

ButtonGroup::ButtonGroup (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                          const juce::String& title, juce::StringArray buttonLegends)
    : param_ (*state.getParameter (paramId)),
      title_ (title.toUpperCase()),
      legends_ (std::move (buttonLegends)),
      attach_ (param_, [this] (float v) { selected_ = juce::roundToInt (v); repaint(); })
{
    attach_.sendInitialUpdate();
    setTooltip (param_.getName (64));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> ButtonGroup::buttonRect (int i) const
{
    // Title, then a row of dots, then the joined segments.
    const auto  r = getLocalBounds().toFloat();
    const float h = r.getHeight();
    const float w = r.getWidth() / static_cast<float> (legends_.size());
    return { r.getX() + w * static_cast<float> (i), r.getY() + h * 0.52f, w, h * 0.40f };
}

void ButtonGroup::paint (juce::Graphics& g)
{
    const auto  r = getLocalBounds().toFloat();
    const float h = r.getHeight();

    g.setColour (theme::text);
    g.setFont (theme::mono (h * 0.17f, true));
    g.drawText (title_, r.withHeight (h * 0.26f), juce::Justification::centred);

    for (int i = 0; i < legends_.size(); ++i)
    {
        const auto b  = buttonRect (i).reduced (1.5f, 0.0f);
        const bool on = i == selected_;

        draw::dot (g, { b.getCentreX(), r.getY() + h * 0.38f }, h * 0.035f, on);

        g.setColour (on ? theme::orange : theme::surface);
        g.fillRect (b);
        g.setColour (on ? theme::orange : theme::hairHi);
        g.drawRect (b, 1.0f);

        g.setColour (on ? theme::bg : theme::textDim);
        g.setFont (theme::mono (b.getHeight() * 0.36f, on));
        g.drawText (legends_[i], b, juce::Justification::centred);
    }
}

void ButtonGroup::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < legends_.size(); ++i)
        if (buttonRect (i).expanded (0.0f, 4.0f).contains (e.position))
        {
            attach_.setValueAsCompleteGesture (static_cast<float> (i));
            return;
        }
}

// ─── PushButton ─────────────────────────────────────────────────────────────

PushButton::PushButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                        const juce::String& legend, Style style, bool momentary)
    : legend_ (legend.toUpperCase()),
      style_ (style),
      momentary_ (momentary),
      attach_ (*state.getParameter (paramId), [this] (float v) { on_ = v > 0.5f; repaint(); })
{
    attach_.sendInitialUpdate();
    setTooltip (state.getParameter (paramId)->getName (64));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PushButton::paint (juce::Graphics& g)
{
    const auto  r   = getLocalBounds().toFloat().reduced (1.0f);
    const bool  lit = on_ || pressed_;

    if (style_ == Style::Accent)
    {
        // Outlined orange at rest, solid orange when latched.
        g.setColour (lit ? theme::orange : theme::bg);
        g.fillRect (r);
        g.setColour (theme::orange);
        g.drawRect (r, 1.5f);
        g.setColour (lit ? theme::bg : theme::orange);
        g.setFont (theme::mono (r.getHeight() * 0.30f, true));
        g.drawText (legend_, r, juce::Justification::centred);
        return;
    }

    if (style_ == Style::Chip)
    {
        g.setColour (pressed_ ? theme::orangeDim : theme::surface);
        g.fillRect (r);
        g.setColour (lit ? theme::orange : theme::hairHi);
        g.drawRect (r, 1.0f);

        const float h = r.getHeight();
        draw::dot (g, { r.getX() + h * 0.5f, r.getCentreY() }, h * 0.12f, lit);
        g.setColour (lit ? theme::orange : theme::textDim);
        g.setFont (theme::mono (h * 0.42f, lit));
        g.drawText (legend_, r.withTrimmedLeft (h * 0.85f).withTrimmedRight (h * 0.2f), juce::Justification::centred);
        return;
    }

    // Plain: dot, hairline button, legend under it.
    const float h = r.getHeight();
    draw::dot (g, { r.getCentreX(), r.getY() + h * 0.1f }, h * 0.06f, lit);

    const auto cap = juce::Rectangle<float> (r.getX(), r.getY() + h * 0.24f, r.getWidth(), h * 0.44f);
    g.setColour (pressed_ ? theme::orange : theme::surface);
    g.fillRect (cap);
    g.setColour (pressed_ ? theme::orange : theme::hairHi);
    g.drawRect (cap, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (theme::mono (h * 0.2f));
    g.drawText (legend_, juce::Rectangle<float> (r.getX() - 10.0f, cap.getBottom() + h * 0.05f, r.getWidth() + 20.0f, h * 0.24f),
                juce::Justification::centred);
}

void PushButton::mouseDown (const juce::MouseEvent&)
{
    pressed_ = true;
    attach_.setValueAsCompleteGesture (momentary_ ? 1.0f : (on_ ? 0.0f : 1.0f));
    repaint();
}

void PushButton::mouseUp (const juce::MouseEvent&)
{
    pressed_ = false;
    repaint();
}
