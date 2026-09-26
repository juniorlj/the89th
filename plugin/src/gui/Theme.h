#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Colours, type and the knob drawing, in one place.

    An original look on purpose: the research flags the hardware's panel
    artwork and layout as the one thing a clone must not copy. Dark instrument
    panel, one warm accent for anything live, a cool second colour for the
    read heads so they separate from the write head at a glance. */
namespace theme
{
inline const juce::Colour bg        { 0xff111214 };
inline const juce::Colour panel     { 0xff191b1e };
inline const juce::Colour panelHi   { 0xff202327 };
inline const juce::Colour line      { 0xff2b2f34 };
inline const juce::Colour lineHi    { 0xff3a3f46 };
inline const juce::Colour text      { 0xffe8e6e1 };
inline const juce::Colour textDim   { 0xff8b9097 };
inline const juce::Colour textFaint { 0xff5c6168 };
inline const juce::Colour accent    { 0xfff2a93b };   // write head, active controls
inline const juce::Colour read      { 0xff5fd0c4 };   // read heads
inline const juce::Colour region    { 0xff8c7cf0 };   // crosspoint region
inline const juce::Colour danger    { 0xffe5654f };

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font mono (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float start, float end, juce::Slider&) override;
};
} // namespace theme
