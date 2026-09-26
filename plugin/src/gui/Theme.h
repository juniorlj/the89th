#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Colour, type and the encoder drawing, in one place.

    Black and orange. The ground is black, structure is hairlines, and orange
    carries everything live: values, lit LED rings, active switches, section
    titles, the display's graphics. White is kept for control names so they
    stay readable; grey only for what is off or secondary.

    All drawn in code so every size stays sharp. */
namespace theme
{
inline const juce::Colour bg        { 0xff000000 };
inline const juce::Colour surface   { 0xff0a0a0a };   // inside boxes, button faces
inline const juce::Colour hairline  { 0xff262626 };
inline const juce::Colour hairHi    { 0xff3a3a3a };
inline const juce::Colour text      { 0xfff2f0ec };
inline const juce::Colour textDim   { 0xff8a8680 };
inline const juce::Colour textFaint { 0xff4a4744 };
inline const juce::Colour orange    { 0xffff6a13 };
inline const juce::Colour orangeDim { 0xff7a3208 };
inline const juce::Colour dotOff    { 0xff242424 };
inline const juce::Colour danger    { 0xffff2d2d };

inline juce::Font mono (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions ("Menlo", size, bold ? juce::Font::bold : juce::Font::plain));
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    /** An encoder: a ring of dots lit orange up to the value, a flat black
        cap and a white pointer. */
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float start, float end, juce::Slider&) override;
};
} // namespace theme
