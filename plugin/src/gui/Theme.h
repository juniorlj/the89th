#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** The panel's materials, type and knob drawing, in one place.

    The design language is late-70s studio hardware as modern boutique makers
    have revived it: a black anodised panel, legends silkscreened in white,
    section boxes ruled in thin white lines, skirted black knobs with a white
    pointer, square push buttons under small LEDs, one cream accent button, a
    small monochrome display, wood end cheeks.

    It borrows the language, not anyone's panel: no layout, logo or typeface
    is taken from a real product, for the same trade-dress reason the research
    gives about the original machine's own front panel. */
namespace theme
{
// Metal and print.
inline const juce::Colour panel      { 0xff121213 };
inline const juce::Colour panelEdge  { 0xff1d1d1f };
inline const juce::Colour print      { 0xffecebe6 };   // silkscreen white
inline const juce::Colour printDim   { 0xff8e8d88 };
inline const juce::Colour rule       { 0xb3ecebe6 };   // section lines

// Controls.
inline const juce::Colour knobBody   { 0xff1a1a1b };
inline const juce::Colour knobSkirt  { 0xff0b0b0c };
inline const juce::Colour button     { 0xff2a2a2c };
inline const juce::Colour buttonTop  { 0xff38383b };
inline const juce::Colour cream      { 0xffe6dcc2 };
inline const juce::Colour creamDark  { 0xffb9ae93 };
inline const juce::Colour ledRed     { 0xffff4331 };
inline const juce::Colour ledRedOff  { 0xff3c1511 };
inline const juce::Colour ledGreen   { 0xff5dff6a };
inline const juce::Colour ledAmber   { 0xffffb52e };

// Display.
inline const juce::Colour oledBg     { 0xff050607 };
inline const juce::Colour oled       { 0xffd9e6ff };
inline const juce::Colour oledDim    { 0x59d9e6ff };
inline const juce::Colour oledFaint  { 0x26d9e6ff };

// Wood.
inline const juce::Colour wood       { 0xff5b3f28 };
inline const juce::Colour woodDark   { 0xff2e1f13 };

/** Section titles and the name: tall, bold, condensed. */
inline juce::Font title (float size)
{
    return juce::Font (juce::FontOptions ("DIN Condensed", size, juce::Font::bold));
}

/** Control legends: small condensed caps. */
inline juce::Font legend (float size)
{
    return juce::Font (juce::FontOptions ("Avenir Next Condensed", size, juce::Font::bold))
               .withExtraKerningFactor (0.06f);
}

/** The display's type. */
inline juce::Font screen (float size)
{
    return juce::Font (juce::FontOptions ("Menlo", size, juce::Font::bold));
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float start, float end, juce::Slider&) override;

    // The value popup shown while a knob turns: a small printed-label box.
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;
};
} // namespace theme
