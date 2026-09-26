#include "Theme.h"

namespace theme
{
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::backgroundColourId, oledBg);
    setColour (juce::TooltipWindow::textColourId, print);
    setColour (juce::TooltipWindow::outlineColourId, rule);
    setColour (juce::BubbleComponent::backgroundColourId, oledBg);
    setColour (juce::BubbleComponent::outlineColourId, rule);
    setColour (juce::TooltipWindow::textColourId, oled);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                    float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h));
    const float size   = std::min (bounds.getWidth(), bounds.getHeight());
    const auto  c      = bounds.getCentre();
    const float angle  = start + pos * (end - start);

    // The scale is printed on the panel, not on the knob: eleven ticks, the
    // ends and centre longer.
    const float scaleR = size * 0.49f;
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * static_cast<float> (i) / 10.0f;
        const bool major = i == 0 || i == 5 || i == 10;
        g.setColour (major ? print : printDim);
        g.drawLine ({ c.getPointOnCircumference (scaleR * (major ? 0.83f : 0.88f), a),
                      c.getPointOnCircumference (scaleR, a) },
                    std::max (1.0f, size * (major ? 0.022f : 0.016f)));
    }

    // Skirt: a slightly wider dark disc under the cap, with a knurled edge.
    const float skirt = size * 0.37f;
    g.setColour (knobSkirt);
    g.fillEllipse (c.x - skirt, c.y - skirt, skirt * 2.0f, skirt * 2.0f);
    g.setColour (juce::Colour (0xff262628));
    for (int i = 0; i < 48; ++i)
    {
        const float a = static_cast<float> (i) / 48.0f * juce::MathConstants<float>::twoPi + angle;
        g.drawLine ({ c.getPointOnCircumference (skirt * 0.90f, a), c.getPointOnCircumference (skirt, a) }, 1.0f);
    }

    // Cap: lit from above, so lighter at the top edge.
    const float cap = size * 0.28f;
    juce::ColourGradient grad (juce::Colour (0xff3a3a3d), c.x, c.y - cap,
                               juce::Colour (0xff111112), c.x, c.y + cap, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f, 1.0f);

    // Pointer: a white line from near the centre to the skirt's edge.
    g.setColour (s.isMouseButtonDown() ? cream : print);
    g.drawLine ({ c.getPointOnCircumference (skirt * 0.25f, angle),
                  c.getPointOnCircumference (skirt * 0.96f, angle) },
                std::max (1.5f, size * 0.04f));
}

juce::Font LookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return screen (13.0f);
}

int LookAndFeel::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;
}

void LookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                              const juce::Rectangle<float>& body)
{
    g.setColour (oledBg);
    g.fillRect (body);
    g.setColour (rule);
    g.drawRect (body, 1.0f);
}
} // namespace theme
