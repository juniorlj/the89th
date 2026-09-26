#include "Controls.h"

// ─── Drawing helpers ────────────────────────────────────────────────────────

namespace draw
{
void led (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour lit, bool on)
{
    if (on)
    {
        g.setColour (lit.withAlpha (0.18f));
        g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
        g.setColour (lit);
    }
    else
    {
        g.setColour (lit.darker (2.6f).withAlpha (0.9f));
    }
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    // A pin-point highlight so an unlit LED still reads as a lens.
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.12f));
    g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.5f, r * 0.4f);
}

void buttonCap (juce::Graphics& g, juce::Rectangle<float> r, bool pressed)
{
    const float corner = r.getHeight() * 0.08f;
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), corner);

    auto cap = pressed ? r.translated (0.0f, 1.0f) : r;
    juce::ColourGradient grad (theme::buttonTop, cap.getX(), cap.getY(),
                               theme::button, cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, corner);
    g.setColour (juce::Colours::white.withAlpha (pressed ? 0.03f : 0.08f));
    g.drawHorizontalLine (juce::roundToInt (cap.getY() + 1.0f), cap.getX() + 2.0f, cap.getRight() - 2.0f);
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
    slider_.setPopupDisplayEnabled (true, true, nullptr, 900);

    auto report = [this]
    {
        if (onTouch)
            onTouch (legend_, param_.getCurrentValueAsText());
    };
    slider_.onDragStart   = report;
    slider_.onValueChange = [this, report] { if (slider_.isMouseButtonDown()) report(); };

    addAndMakeVisible (slider_);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (juce::roundToInt (r.getHeight() * 0.22f));
    const int side = std::min (r.getWidth(), r.getHeight());
    slider_.setBounds (r.withSizeKeepingCentre (side, side));

    // Show the popup inside the editor rather than as a separate desktop
    // window, which some hosts place badly.
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
        slider_.setPopupDisplayEnabled (true, true, top, 900);
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto legendArea = r.removeFromBottom (r.getHeight() * 0.22f);
    const auto knob = slider_.getBounds().toFloat();

    g.setColour (theme::print);
    g.setFont (theme::legend (legendArea.getHeight() * 0.62f));
    g.drawFittedText (legend_, legendArea.toNearestInt(), juce::Justification::centred, 1);

    if (minMark_.isNotEmpty() || maxMark_.isNotEmpty())
    {
        // Printed under the ends of the scale, kept inside the component so
        // nothing clips.
        const auto full = getLocalBounds().toFloat();
        const float mh  = knob.getHeight() * 0.15f;
        const float y   = knob.getBottom() - mh;
        g.setColour (theme::printDim);
        g.setFont (theme::legend (mh * 0.85f));
        g.drawText (minMark_, juce::Rectangle<float> (full.getX(), y, full.getWidth() * 0.4f, mh),
                    juce::Justification::centredLeft);
        g.drawText (maxMark_, juce::Rectangle<float> (full.getRight() - full.getWidth() * 0.4f, y, full.getWidth() * 0.4f, mh),
                    juce::Justification::centredRight);
    }
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
    // Title across the top, then per button: LED, cap, legend.
    const auto r   = getLocalBounds().toFloat();
    const float h  = r.getHeight();
    const int   n  = legends_.size();
    const float cw = r.getWidth() / static_cast<float> (n);
    const float bw = std::min (cw * 0.78f, h * 0.62f);
    const float bh = h * 0.30f;
    return { r.getX() + cw * (static_cast<float> (i) + 0.5f) - bw * 0.5f, r.getY() + h * 0.40f, bw, bh };
}

void ButtonGroup::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();

    g.setColour (theme::print);
    g.setFont (theme::title (h * 0.24f));
    g.drawText (title_, r.withHeight (h * 0.22f), juce::Justification::centred);

    for (int i = 0; i < legends_.size(); ++i)
    {
        const auto b  = buttonRect (i);
        const bool on = i == selected_;

        draw::led (g, { b.getCentreX(), r.getY() + h * 0.31f }, h * 0.045f, theme::ledRed, on);
        draw::buttonCap (g, b, on);

        g.setColour (on ? theme::print : theme::printDim);
        g.setFont (theme::legend (h * 0.16f));
        g.drawText (legends_[i], juce::Rectangle<float> (b.getX() - 10.0f, b.getBottom() + h * 0.03f,
                                                         b.getWidth() + 20.0f, h * 0.2f),
                    juce::Justification::centred);
    }
}

void ButtonGroup::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < legends_.size(); ++i)
        if (buttonRect (i).expanded (4.0f).contains (e.position))
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
    const auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();

    // LED on top, cap below, legend printed on the cap for the cream button
    // and under it for the dark one.
    const bool ledLit = on_ || (momentary_ && pressed_);
    draw::led (g, { r.getCentreX(), r.getY() + h * 0.09f }, h * 0.055f, theme::ledRed, ledLit);

    auto cap = juce::Rectangle<float> (r.getX() + 2.0f, r.getY() + h * 0.22f, r.getWidth() - 4.0f, h * 0.52f);

    if (style_ == Style::Cream)
    {
        const float corner = cap.getHeight() * 0.06f;
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), corner);
        if (pressed_) cap = cap.translated (0.0f, 1.2f);
        juce::ColourGradient grad (theme::cream, cap.getX(), cap.getY(), theme::creamDark, cap.getX(), cap.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, corner);
        g.setColour (juce::Colour (0xff2a241a));
        g.setFont (theme::title (cap.getHeight() * 0.42f));
        g.drawText (legend_, cap, juce::Justification::centred);
    }
    else
    {
        draw::buttonCap (g, cap, pressed_);
        g.setColour (theme::print);
        g.setFont (theme::legend (h * 0.2f));
        g.drawText (legend_, juce::Rectangle<float> (r.getX() - 10.0f, cap.getBottom() + h * 0.04f, r.getWidth() + 20.0f, h * 0.22f),
                    juce::Justification::centred);
    }
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
