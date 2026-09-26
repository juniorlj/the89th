#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Telemetry.h"

/** One channel's display: a small monochrome screen set into the panel.

    It draws the channel's memory as a ring. The write head sweeps round it at
    the clock rate, and everything else sits behind it by delay, so the picture
    turns exactly as the machine's addresses do:
    - the crosspoint region, as a heavy arc;
    - the read heads, as square pixels, each as bright as it is loud;
    - in delay mode, the span from the read head back to the write head.

    Text runs along the top (mode, pitch or delay) and the bottom (the last
    control touched, or else the region and how well the last join matched),
    the way a hardware display shows the parameter under your hand. */
class MemoryRing final : public juce::Component
{
public:
    MemoryRing (const Telemetry&, int channel, juce::String title);

    /** Pull fresh telemetry and repaint. Called by the editor's timer. */
    void refresh();

    /** Show a control's name and value on the bottom line for a moment. */
    void showTouched (const juce::String& legend, const juce::String& value);

    void paint (juce::Graphics&) override;

private:
    struct View
    {
        float writePos = 0.0f, primary = 0.0f, secondary = 0.0f;
        float gainA = 1.0f, gainB = 0.0f, lo = 0.0f, hi = 0.0f, match = 0.0f;
        float msPerWord = 0.04f, level = 0.0f, rate = 1.0f, delayMs = 0.0f;
        int   words = 8192;
        bool  traversal = true, splicing = false, reversed = false;
        bool  frozen = false, delayMode = false, quasi = false;
    };

    float angleOf (float delayWords) const;

    const Telemetry& t_;
    int channel_;
    juce::String title_;
    View v_;
    float spliceFlash_ = 0.0f;
    bool  wasSplicing_ = false;

    juce::String touchedText_;
    int touchedTicks_ = 0;
};
