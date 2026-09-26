#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Controls.h"
#include "gui/MemoryRing.h"
#include "gui/Theme.h"

#include <array>
#include <memory>

class The89thProcessor;

/** The panel.

    Laid out on a 1100 x 680 design grid and scaled as a whole, so every
    proportion holds from the smallest to the largest window. Top: name, build
    stamp, Init. Then the machine's switches, the latch and the mix. Below, one
    panel per channel: its live memory ring and its controls.

    Controls that do nothing in the current mode fade back rather than hide, so
    the panel never rearranges under your hand. */
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

        MemoryRing ring;
        Knob delay, pitch, xp1, xp2, feedback, vibDepth, vibRate;
    };

    The89thProcessor& proc_;
    theme::LookAndFeel lnf_;
    juce::TooltipWindow tooltips_ { this, 700 };

    SegmentedSwitch mode_, stereo_, range_, bandwidth_;
    LatchButton freeze_, init_;
    Knob mix_;
    std::array<std::unique_ptr<ChannelUI>, 2> ch_;

    std::array<juce::Rectangle<int>, 2> channelPanels_;
    juce::Rectangle<int> header_, strip_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thEditor)
};
