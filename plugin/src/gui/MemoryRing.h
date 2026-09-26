#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Telemetry.h"

/** One channel's memory drawn as a circle, live.

    The ring is the RAM. The write head sweeps round it at the clock rate;
    everything else is placed behind it by delay, so the picture turns with the
    write head exactly as the addresses do in the machine:

    - the crosspoint region, as an arc;
    - the carrying read head, and during a splice the incoming one, each drawn
      as bright as it is loud;
    - in delay mode, the single read head and the span back to the write head.

    When latched, the write head stops and the ring dims except the region still
    being played. The centre reads out what the channel is doing. */
class MemoryRing final : public juce::Component
{
public:
    MemoryRing (const Telemetry&, int channel, juce::String title);

    /** Pull fresh telemetry and repaint. Called by the editor's timer. */
    void refresh();

    void paint (juce::Graphics&) override;

private:
    struct View
    {
        float writePos = 0.0f, primary = 0.0f, secondary = 0.0f;
        float gainA = 1.0f, gainB = 0.0f, lo = 0.0f, hi = 0.0f, match = 0.0f;
        float msPerWord = 0.04f, level = 0.0f;
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
};
