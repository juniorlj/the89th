#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Controls.h"
#include "gui/MemoryRing.h"
#include "gui/PresetBar.h"
#include "gui/Theme.h"

#include <array>
#include <memory>
#include <vector>

class The89thProcessor;

/** The panel.

    Laid out on a 1100 x 800 design grid and scaled as a whole. Black ground,
    hairline structure, orange for everything live. Along the top: the name,
    the preset strip with A/B, the build stamp, and Init. Then SYSTEM (the
    machine's shared switches), OUTPUT and KEYS. Then the modern row: FEEDBACK
    LOOP, MUSICAL and SCRUB. Below, one section per channel: its Mode and
    Memory Latch at the outer edge, as the hardware has them, its display, then
    the read controls and the recirculation and vibrato controls. Link sits on
    channel 2's frame, since it is channel 2 that follows.

    The KEYBOARD chip in KEYS swaps the channel sections for the original keyboard's
    panel: PITCH RATIO, ENVELOPE, VIBRATO, MEMORY SYNCHRO and REVERSE SYNCHRO.

    Controls the current mode ignores fade back rather than hide, so nothing
    moves under your hand. */
class The89thEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    static constexpr int kBaseW = 1100;
    static constexpr int kBaseH = 800;

    explicit The89thEditor (The89thProcessor&);
    ~The89thEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pull telemetry and update the live parts. Runs on the timer; exposed so
        the snapshot tool can draw a settled frame without waiting on one. */
    void refresh();

    /** Swap the channel sections for the keyboard's panel, or back. */
    void showKeyboardPage (bool);

private:
    void timerCallback() override { refresh(); }

    struct ChannelUI
    {
        ChannelUI (The89thProcessor&, int channel);

        MemoryRing display;
        ButtonGroup mode;
        PushButton latch;
        Knob delay, pitch, fine, xp1, xp2, feedback, vibDepth, vibRate;
        ButtonGroup vibShape;

        std::vector<juce::Component*> controls();
    };

    /** A ruled section box with its title set into the top line. */
    void section (juce::Graphics&, juce::Rectangle<float>, const juce::String& title) const;

    /** A group title centred over a span, with a rule either side. */
    void groupTitle (juce::Graphics&, juce::Rectangle<float> span, const juce::String& title) const;

    The89thProcessor& proc_;
    theme::LookAndFeel lnf_;
    juce::TooltipWindow tooltips_ { this, 700 };

    PresetBar presetBar_;

    ButtonGroup stereo_, range_, bandwidth_;
    PushButton init_;
    Knob mix_;

    ButtonGroup route_, snap_, scrubMode_;
    Knob lowCut_, highCut_, drive_, scrubDepth_, scrubRate_;
    PushButton sync_, link_;

    ButtonGroup keys_, kbPlay_;
    Knob keysRoot_;
    ChipToggle kbPage_ { "Keyboard" };
    std::array<std::unique_ptr<ChannelUI>, 2> ch_;

    /** The keyboard's page. */
    struct KeyboardUI
    {
        explicit KeyboardUI (The89thProcessor&);

        Knob trim, slope, added;
        PushButton env;
        Knob attack, hold, release;
        PushButton vib;
        Knob vibRate, vibSharp, vibDepth, modRate, modSharp, modDepth, vibAttack, vibRelease;
        ButtonGroup synchro;
        Knob attackPt, returnPt, endPt, speed;
        ButtonGroup reverse;
        PushButton gate;
        Knob thresh, revDelay;

        std::vector<juce::Component*> controls();
    };
    KeyboardUI kb_;

    /** Where the synchro display sits, in design units. */
    static juce::Rectangle<float> synchroBar();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (The89thEditor)
};
