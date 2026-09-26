#include "Theme.h"

namespace theme
{
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, panelHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                    float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h));
    const float size   = std::min (bounds.getWidth(), bounds.getHeight());
    const auto  centre = bounds.getCentre();
    const float radius = size * 0.5f - size * 0.06f;
    const float stroke = std::max (2.0f, size * 0.045f);

    // Bipolar controls (pitch) fill from their centre, where unity sits.
    const bool  bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float angle   = start + pos * (end - start);
    const float from    = bipolar ? start + 0.5f * (end - start) : start;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         std::min (from, angle), std::max (from, angle), true);
    const bool active = s.isEnabled();
    g.setColour (active ? accent : textFaint);
    g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Body.
    const float body = radius - stroke * 2.2f;
    g.setColour (s.isMouseOverOrDragging() ? panelHi.brighter (0.06f) : panelHi);
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour (lineHi);
    g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.0f);

    // Pointer.
    const auto tip  = centre.getPointOnCircumference (body * 0.82f, angle);
    const auto root = centre.getPointOnCircumference (body * 0.30f, angle);
    g.setColour (text);
    g.drawLine ({ root, tip }, std::max (1.5f, size * 0.03f));
}
} // namespace theme
