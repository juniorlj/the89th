#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Controls.h"
#include "gui/MemoryRing.h"
#include "gui/Theme.h"

#include <array>
#include <memory>

class The89thProcessor;

/** The front panel.

    Laid out on a 1100 x 680 design grid and scaled as a whole. Wood cheeks
    either side of a black panel. Along the top: the name, a serial plate with
    the build stamp, and Init. Then three ruled sections: SYSTEM (the machine's
    switches), LATCH (the one cream button) and OUTPUT (mix). Below, one
    section per channel: its display, then the read controls and the
    recirculation and vibrato controls under printed group titles.

    Controls the current mode ignores fade back rather than hide, so nothing
    moves under your hand. */
class The89thEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    static constexpr int kBaseW = 1100;
    static constexpr int kBaseH = 680;

    explicit The89thEditor (The89thProcessor&);
    ~The89thEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pull telemetry and update the live parts. Runs on the timer; exposed so
        the snapshot tool can draw a settled frame without waiting on one. */
    void refresh();

private:
    void timerCallback() override { refresh(); }

    struct ChannelUI
    {
        ChannelUI (The89thProcessor&, int channel);

        MemoryRing display;
        Knob delay, pitch, xp1, xp2, feedback, vibDepth, vibRate;
    };

    /** A ruled section box with its title set into the top line. */
    void section (juce::Graphics&, juce::Rectangle<float>, const juce::String& title) const;

    /** A group title centred over a span, with a rule either side. */
    void groupTitle (juce::Graphics&, juce::Rectangle<float> span, const juce::String& title) const;

    void rebuildWood();

    The89thProcessor& proc_;
    theme::LookAndFeel lnf_;
    juce::TooltipWindow tooltips_ { this, 700 };

    ButtonGroup mode_, stereo_, range_, bandwidth_;
    PushButton freeze_, init_;
    Knob mix_;
    std::array<std::unique_ptr<ChannelUI>, 2> ch_;

    juce::Image wood_;   // one cheek, cached per size

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thEditor)
};
