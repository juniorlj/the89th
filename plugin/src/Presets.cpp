#include "Presets.h"
#include "ParameterIDs.h"

namespace
{
constexpr const char* kExt = ".the89th";

/** Knob position for a note value under Sync: 1/64 is step 1, 1/1 step 17. */
float syncKnob (int step) { return static_cast<float> (step) / 17.0f; }

float semis (double s) { return static_cast<float> (std::exp2 (s / 12.0)); }

using V = PresetManager::Values;

/** Each preset lists only what it changes from the defaults. Names read as
    the sound, not the settings. */
std::vector<std::pair<juce::String, V>> buildFactory()
{
    using namespace pid;
    const auto& L = channel[0];
    const auto& R = channel[1];

    return {
        { "Init", {} },

        { "Octave Up", {
            { L.pitch, 2.0f }, { R.pitch, 2.0f },
            { L.crosspoint2, 0.3f }, { R.crosspoint2, 0.3f },
        } },

        { "Octave Down", {
            { L.pitch, 0.5f }, { R.pitch, 0.5f },
            { L.crosspoint2, 0.4f }, { R.crosspoint2, 0.4f },
        } },

        { "Fifth and Fourth", {
            { snap, 1.0f },
            { L.pitch, semis (7) }, { R.pitch, semis (-5) },
            { L.crosspoint2, 0.25f }, { R.crosspoint2, 0.3f },
            { mix, 0.5f },
        } },

        { "Reverse", {
            { L.crosspoint1, 0.5f }, { L.crosspoint2, 0.0f },
            { R.crosspoint1, 0.45f }, { R.crosspoint2, 0.0f },
            { mix, 0.6f },
        } },

        { "Slapback", {
            { L.mode, 0.0f }, { R.mode, 0.0f },
            { L.delay, 0.30f }, { R.delay, 0.34f },
            { L.feedback, 0.15f }, { R.feedback, 0.15f },
            { mix, 0.35f },
        } },

        { "Doubler", {
            { L.mode, 0.0f }, { R.mode, 0.0f }, { range, 1.0f },
            { L.delay, 0.55f }, { R.delay, 0.8f },
            { L.vibratoDepth, 0.08f }, { L.vibratoRate, 0.7f },
            { R.vibratoDepth, 0.08f }, { R.vibratoRate, 0.9f },
            { mix, 0.5f },
        } },

        { "Ping-Pong Eighths", {
            { L.mode, 0.0f }, { R.mode, 0.0f }, { pid::sync, 1.0f }, { fbRoute, 1.0f },
            { L.delay, syncKnob (9) }, { R.delay, syncKnob (9) },
            { L.feedback, 0.55f }, { R.feedback, 0.55f },
            { highCut, 6000.0f },
            { mix, 0.4f },
        } },

        { "Rising Arpeggio", {
            { snap, 1.0f },
            { L.pitch, semis (7) }, { R.pitch, semis (12) },
            { L.crosspoint2, 0.35f }, { R.crosspoint2, 0.35f },
            { L.feedback, 0.7f }, { R.feedback, 0.6f },
            { mix, 0.5f },
        } },

        { "Falling Tape", {
            { L.pitch, semis (-1) }, { R.pitch, semis (-1) },
            { L.feedback, 0.75f }, { R.feedback, 0.75f },
            { highCut, 4000.0f }, { drive, 0.4f },
            { mix, 0.5f },
        } },

        { "Shimmer", {
            { stereo, 1.0f }, { bandwidth, 2.0f },
            { L.pitch, 2.0f }, { R.pitch, 2.0f }, { R.fine, 7.0f },
            { L.crosspoint2, 0.4f }, { R.crosspoint2, 0.35f },
            { L.feedback, 0.6f }, { R.feedback, 0.6f },
            { lowCut, 300.0f },
            { mix, 0.4f },
        } },

        { "Trill", {
            { L.vibratoShape, 1.0f }, { R.vibratoShape, 1.0f },
            { L.vibratoDepth, 2.0f }, { R.vibratoDepth, 2.0f },
            { L.vibratoRate, 6.0f }, { R.vibratoRate, 6.0f },
            { L.crosspoint2, 0.2f }, { R.crosspoint2, 0.2f },
        } },

        { "Scrubbed Loop", {
            { bandwidth, 0.0f },
            { L.crosspoint1, 0.2f }, { L.crosspoint2, 0.32f },
            { R.crosspoint1, 0.25f }, { R.crosspoint2, 0.36f },
            { scrubDepth, 0.8f }, { scrubRate, 0.3f }, { scrubMode, 1.0f },
            { mix, 0.7f },
        } },

        { "Lo-Fi Echo", {
            { L.mode, 0.0f }, { R.mode, 0.0f }, { bandwidth, 0.0f },
            { L.delay, 0.4f }, { R.delay, 0.45f },
            { L.feedback, 0.5f }, { R.feedback, 0.5f },
            { drive, 0.35f },
            { mix, 0.45f },
        } },
    };
}
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, juce::File folder)
    : state_ (state), folder_ (std::move (folder))
{
    refresh();
}

juce::File PresetManager::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("THE89TH").getChildFile ("Presets");
}

const std::vector<std::pair<juce::String, PresetManager::Values>>& PresetManager::factory()
{
    static const auto presets = buildFactory();
    return presets;
}

bool PresetManager::stored (const juce::String& id)
{
    return id != pid::init && id != pid::build;
}

const std::vector<PresetManager::Entry>& PresetManager::refresh()
{
    entries_.clear();
    for (const auto& [name, values] : factory())
        entries_.push_back ({ name, true, {} });

    auto files = folder_.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExt);
    files.sort();
    for (const auto& f : files)
        entries_.push_back ({ f.getFileNameWithoutExtension(), false, f });

    // Keep pointing at the same preset if it still exists.
    current_ = -1;
    for (int i = 0; i < static_cast<int> (entries_.size()); ++i)
        if (entries_[static_cast<std::size_t> (i)].name == currentName_)
            current_ = i;

    return entries_;
}

PresetManager::Values PresetManager::capture() const
{
    Values v;
    for (auto* raw : state_.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (raw))
            if (stored (p->paramID))
                v[p->paramID] = p->convertFrom0to1 (p->getValue());
    return v;
}

void PresetManager::apply (const Values& given)
{
    // Presets saved while Mode and the latch were global carry only the left
    // side's IDs; the right side takes the same setting.
    Values values = given;
    for (int k = 0; k < 2; ++k)
    {
        const juce::String left  = k == 0 ? pid::channel[0].mode : pid::channel[0].freeze;
        const juce::String right = k == 0 ? pid::channel[1].mode : pid::channel[1].freeze;
        if (values.count (left) != 0 && values.count (right) == 0)
            values[right] = values[left];
    }

    for (auto* raw : state_.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (raw))
        {
            if (! stored (p->paramID))
                continue;

            const auto it = values.find (p->paramID);
            const float target = it != values.end() ? p->convertTo0to1 (it->second) : p->getDefaultValue();

            p->beginChangeGesture();
            p->setValueNotifyingHost (target);
            p->endChangeGesture();
        }
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= static_cast<int> (entries_.size()))
        return false;

    const auto& e = entries_[static_cast<std::size_t> (index)];
    Values values;

    if (e.factory)
    {
        values = factory()[static_cast<std::size_t> (index)].second;
    }
    else
    {
        const auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr)
            return false;
        values = treeToValues (juce::ValueTree::fromXml (*xml));
    }

    apply (values);
    current_     = index;
    currentName_ = e.name;
    loaded_      = capture();
    return true;
}

bool PresetManager::loadNext (int direction)
{
    const int n = static_cast<int> (entries_.size());
    if (n == 0)
        return false;
    const int from = current_ < 0 ? 0 : current_;
    return load (((from + direction) % n + n) % n);
}

bool PresetManager::save (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;
    for (const auto& [factoryName, values] : factory())
        if (factoryName.equalsIgnoreCase (name))
            return false;

    if (! folder_.createDirectory())
        return false;

    const auto tree = valuesToTree (capture(), "THE89TH_PRESET").setProperty ("name", name, nullptr);
    const auto xml  = tree.createXml();
    if (xml == nullptr || ! xml->writeTo (folder_.getChildFile (name + kExt)))
        return false;

    currentName_ = name;
    loaded_      = capture();
    refresh();
    return true;
}

bool PresetManager::remove (int index)
{
    if (index < 0 || index >= static_cast<int> (entries_.size()))
        return false;
    const auto& e = entries_[static_cast<std::size_t> (index)];
    if (e.factory || ! e.file.deleteFile())
        return false;
    refresh();
    return true;
}

bool PresetManager::modified() const
{
    const auto now = capture();
    const auto& ref = loaded_.empty() ? factory().front().second : loaded_;

    for (auto* raw : state_.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (raw))
        {
            if (! stored (p->paramID))
                continue;
            const auto it = ref.find (p->paramID);
            const float want = it != ref.end() ? it->second : p->convertFrom0to1 (p->getDefaultValue());
            const float span = p->getNormalisableRange().getRange().getLength();
            if (std::abs (now.at (p->paramID) - want) > 1.0e-4f * span)
                return true;
        }
    return false;
}

void PresetManager::selectSlot (int slot)
{
    slot = slot != 0 ? 1 : 0;
    if (slot == active_)
        return;

    auto& leaving = slots_[static_cast<std::size_t> (active_)];
    auto& target  = slots_[static_cast<std::size_t> (slot)];
    leaving = capture();
    if (target.empty())
        target = leaving;

    active_ = slot;
    apply (target);
}

void PresetManager::copyToOtherSlot()
{
    slots_[static_cast<std::size_t> (1 - active_)] = capture();
}

juce::ValueTree PresetManager::valuesToTree (const Values& values, const juce::String& type)
{
    juce::ValueTree t (type);
    for (const auto& [id, v] : values)
        t.appendChild (juce::ValueTree ("V").setProperty ("id", id, nullptr)
                                            .setProperty ("value", v, nullptr), nullptr);
    return t;
}

PresetManager::Values PresetManager::treeToValues (const juce::ValueTree& t)
{
    Values v;
    for (const auto& child : t)
        if (child.hasProperty ("id"))
            v[child["id"].toString()] = static_cast<float> (child["value"]);
    return v;
}

juce::ValueTree PresetManager::toTree() const
{
    juce::ValueTree t (kTreeType);
    t.setProperty ("name", currentName_, nullptr);
    t.setProperty ("slot", active_, nullptr);
    t.appendChild (valuesToTree (slots_[0], "A"), nullptr);
    t.appendChild (valuesToTree (slots_[1], "B"), nullptr);
    t.appendChild (valuesToTree (loaded_, "LOADED"), nullptr);
    return t;
}

void PresetManager::fromTree (const juce::ValueTree& t)
{
    if (! t.hasType (kTreeType))
        return;

    currentName_ = t.getProperty ("name", "Init").toString();
    active_      = static_cast<int> (t.getProperty ("slot", 0)) != 0 ? 1 : 0;
    slots_[0]    = treeToValues (t.getChildWithName ("A"));
    slots_[1]    = treeToValues (t.getChildWithName ("B"));
    loaded_      = treeToValues (t.getChildWithName ("LOADED"));
    refresh();
}
