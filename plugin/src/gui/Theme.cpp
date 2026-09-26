#include "Theme.h"

namespace theme
{
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::TooltipWindow::backgroundColourId, surface);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, hairHi);

    // Preset list and the save dialog.
    setColour (juce::PopupMenu::backgroundColourId, surface);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, orange);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
    setColour (juce::PopupMenu::highlightedTextColourId, bg);
    setColour (juce::AlertWindow::backgroundColourId, surface);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, hairHi);
    setColour (juce::TextEditor::backgroundColourId, bg);
    setColour (juce::TextEditor::textColourId, orange);
    setColour (juce::TextEditor::outlineColourId, hairHi);
    setColour (juce::TextEditor::focusedOutlineColourId, orange);
    setColour (juce::TextEditor::highlightColourId, orangeDim);
    setColour (juce::CaretComponent::caretColourId, orange);
    setColour (juce::TextButton::buttonColourId, surface);
    setColour (juce::TextButton::buttonOnColourId, orange);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ComboBox::outlineColourId, hairHi);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                    float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h));
    const float size  = std::min (bounds.getWidth(), bounds.getHeight());
    const auto  c     = bounds.getCentre();
    const float angle = start + pos * (end - start);

    // LED ring. Bipolar controls (pitch) light from the centre, where unity is.
    constexpr int kDots = 25;
    const bool  bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float ringR   = size * 0.46f;
    const float dotR    = std::max (1.2f, size * 0.028f);

    for (int i = 0; i < kDots; ++i)
    {
        const float t = static_cast<float> (i) / (kDots - 1);
        const float a = start + t * (end - start);

        bool lit;
        if (bipolar)
            lit = (pos >= 0.5f) ? (t >= 0.5f - 0.001f && t <= pos + 0.001f)
                                : (t <= 0.5f + 0.001f && t >= pos - 0.001f);
        else
            lit = t <= pos + 0.001f && pos > 0.0005f;

        const auto p = c.getPointOnCircumference (ringR, a);
        g.setColour (lit ? orange : dotOff);
        g.fillEllipse (p.x - dotR, p.y - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    // Cap: flat black disc, hairline edge.
    const float cap = size * 0.34f;
    g.setColour (juce::Colour (0xff111111));
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);
    g.setColour (s.isMouseOverOrDragging() ? hairHi.brighter (0.3f) : hairHi);
    g.drawEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f, 1.0f);

    // Pointer.
    g.setColour (s.isMouseButtonDown() ? orange : text);
    g.drawLine ({ c.getPointOnCircumference (cap * 0.30f, angle),
                  c.getPointOnCircumference (cap * 0.86f, angle) },
                std::max (1.5f, size * 0.035f));
}
} // namespace theme
