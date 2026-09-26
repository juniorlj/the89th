#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <functional>
#include <map>
#include <vector>

/** Presets and A/B compare, over the processor's parameters.

    A preset is a set of plain parameter values. Loading one first puts every
    parameter back to its default, then applies the preset's values, so a
    preset only has to list what it changes and stays valid when later
    versions add parameters. Init and Build are never stored.

    Factory presets live in code. User presets are XML files in the preset
    folder, one per preset, named after it.

    A/B holds two complete settings. Switching stores what is on the panel into
    the slot you are leaving and loads the other; an empty slot starts as a
    copy of the current settings. Both slots and the preset name are saved with
    the host project.

    Message thread only. */
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        bool         factory = false;
        juce::File   file;  // user presets only
    };

    using Values = std::map<juce::String, float>;  // parameter ID -> plain value

    PresetManager (juce::AudioProcessorValueTreeState& state, juce::File folder = defaultFolder());

    /** ~/Library/Application Support/THE89TH/Presets on a Mac. */
    static juce::File defaultFolder();

    static const std::vector<std::pair<juce::String, Values>>& factory();

    /** Factory first, then user presets alphabetically. Rescans the folder. */
    const std::vector<Entry>& refresh();
    const std::vector<Entry>& entries() const noexcept { return entries_; }

    bool load (int index);
    bool loadNext (int direction);  // +1 or -1, wrapping

    /** Writes the panel as a user preset, replacing one of the same name.
        Factory names are refused, so a factory preset can't be shadowed. */
    bool save (const juce::String& name);
    bool remove (int index);  // user presets only

    int                 currentIndex() const noexcept { return current_; }
    const juce::String& currentName()  const noexcept { return currentName_; }

    /** True when the panel no longer matches what was last loaded or saved. */
    bool modified() const;

    // ─── A/B ────────────────────────────────────────────────────────────────
    int  activeSlot() const noexcept { return active_; }
    void selectSlot (int slot);
    void copyToOtherSlot();  // the panel into the slot you are not on

    // ─── Saving with the project ────────────────────────────────────────────
    juce::ValueTree toTree() const;
    void            fromTree (const juce::ValueTree&);
    static constexpr const char* kTreeType = "PRESETS";

    Values capture() const;
    void   apply (const Values&);

private:
    static bool stored (const juce::String& paramID);
    static juce::ValueTree valuesToTree (const Values&, const juce::String& type);
    static Values          treeToValues (const juce::ValueTree&);

    juce::AudioProcessorValueTreeState& state_;
    juce::File folder_;

    std::vector<Entry> entries_;
    int          current_ = 0;
    juce::String currentName_ { "Init" };
    Values       loaded_;

    int active_ = 0;
    std::array<Values, 2> slots_ {};
};
