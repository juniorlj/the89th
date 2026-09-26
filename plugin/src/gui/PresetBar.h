#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Presets.h"
#include "Theme.h"

/** The header's preset strip: previous, the preset name (click for the list),
    next, Save, then the A/B compare and a copy across.

    The name shows an asterisk once the panel no longer matches the preset.
    Drawn as one component with hit regions, like the selectors. */
class PresetBar final : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    explicit PresetBar (PresetManager&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Poll the manager and repaint if anything shown has changed. */
    void refresh();

private:
    enum class Hit { None, Prev, Name, Next, Save, SlotA, SlotB, Copy };

    juce::Rectangle<float> area (Hit) const;
    Hit hitAt (juce::Point<float>) const;

    void showMenu();
    void askToSave();

    PresetManager& presets_;
    Hit hover_ = Hit::None, pressed_ = Hit::None;

    juce::String shownName_;
    bool shownModified_ = false;
    int  shownSlot_ = 0;
};
