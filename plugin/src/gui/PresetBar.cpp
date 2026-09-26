#include "PresetBar.h"

namespace
{
enum MenuIds
{
    kSaveAs     = 10000,
    kDelete     = 10001,
    kShowFolder = 10002,
};
} // namespace

PresetBar::PresetBar (PresetManager& p)
    : presets_ (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refresh();
}

// Layout on the bar's own height: squares for the arrows and the slots.
juce::Rectangle<float> PresetBar::area (Hit h) const
{
    const auto  r    = getLocalBounds().toFloat();
    const float u    = r.getHeight();
    const float save = u * 1.9f;
    const float copy = u * 1.5f;
    const float gap  = u * 0.35f;

    const float slotsX = r.getRight() - copy - gap * 0.5f - 2.0f * u;
    const float saveX  = slotsX - gap - save;
    const float nextX  = saveX - gap * 0.5f - u;

    switch (h)
    {
        case Hit::Prev:  return { r.getX(), r.getY(), u, u };
        case Hit::Name:  return { r.getX() + u, r.getY(), nextX - r.getX() - u, u };
        case Hit::Next:  return { nextX, r.getY(), u, u };
        case Hit::Save:  return { saveX, r.getY(), save, u };
        case Hit::SlotA: return { slotsX, r.getY(), u, u };
        case Hit::SlotB: return { slotsX + u, r.getY(), u, u };
        case Hit::Copy:  return { r.getRight() - copy, r.getY(), copy, u };
        case Hit::None:  break;
    }
    return {};
}

PresetBar::Hit PresetBar::hitAt (juce::Point<float> p) const
{
    for (auto h : { Hit::Prev, Hit::Name, Hit::Next, Hit::Save, Hit::SlotA, Hit::SlotB, Hit::Copy })
        if (area (h).contains (p))
            return h;
    return Hit::None;
}

void PresetBar::refresh()
{
    const auto name     = presets_.currentName();
    const bool modified = presets_.modified();
    const int  slot     = presets_.activeSlot();

    if (name != shownName_ || modified != shownModified_ || slot != shownSlot_)
    {
        shownName_     = name;
        shownModified_ = modified;
        shownSlot_     = slot;
        repaint();
    }
}

void PresetBar::paint (juce::Graphics& g)
{
    const float u = static_cast<float> (getHeight());

    auto box = [&] (Hit h, bool lit)
    {
        const auto r = area (h).reduced (0.5f);
        const bool down = pressed_ == h;
        g.setColour (lit || down ? theme::orange : theme::surface);
        g.fillRect (r);
        g.setColour (lit || down ? theme::orange : (hover_ == h ? theme::textFaint : theme::hairHi));
        g.drawRect (r, 1.0f);
        return r;
    };

    auto arrow = [&] (Hit h, bool left)
    {
        const auto r = box (h, false);
        const auto c = r.getCentre();
        const float s = u * 0.14f;
        juce::Path p;
        if (left)
            p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y);
        else
            p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y);
        g.setColour (hover_ == h ? theme::orange : theme::textDim);
        g.fillPath (p);
    };

    arrow (Hit::Prev, true);
    arrow (Hit::Next, false);

    // Name: a label, the preset in orange, and a caret for the list.
    {
        const auto r = box (Hit::Name, false).reduced (u * 0.3f, 0.0f);
        g.setColour (theme::textFaint);
        g.setFont (theme::mono (u * 0.26f));
        g.drawText ("PRESET", r.withWidth (u * 1.6f), juce::Justification::centredLeft);

        auto nameArea = r.withTrimmedLeft (u * 1.7f).withTrimmedRight (u * 0.5f);
        g.setColour (theme::orange);
        g.setFont (theme::mono (u * 0.38f, true));
        g.drawFittedText (shownName_.toUpperCase() + (shownModified_ ? " *" : ""), nameArea.toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);

        const auto c = juce::Point<float> (r.getRight() - u * 0.18f, r.getCentreY());
        juce::Path caret;
        caret.addTriangle (c.x - u * 0.12f, c.y - u * 0.06f, c.x + u * 0.12f, c.y - u * 0.06f, c.x, c.y + u * 0.1f);
        g.setColour (hover_ == Hit::Name ? theme::orange : theme::textDim);
        g.fillPath (caret);
    }

    auto label = [&] (Hit h, const juce::String& text, bool lit)
    {
        const auto r = box (h, lit);
        g.setColour (lit || pressed_ == h ? theme::bg : (hover_ == h ? theme::text : theme::textDim));
        g.setFont (theme::mono (u * 0.32f, lit));
        g.drawText (text, r, juce::Justification::centred);
    };

    label (Hit::Save,  "SAVE", false);
    label (Hit::SlotA, "A", shownSlot_ == 0);
    label (Hit::SlotB, "B", shownSlot_ == 1);
    label (Hit::Copy,  shownSlot_ == 0 ? "A>B" : "B>A", false);
}

void PresetBar::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitAt (e.position);
    if (h != hover_)
    {
        hover_ = h;
        switch (h)
        {
            case Hit::Prev:  setTooltip ("Previous preset"); break;
            case Hit::Next:  setTooltip ("Next preset"); break;
            case Hit::Name:  setTooltip ("All presets"); break;
            case Hit::Save:  setTooltip ("Save the panel as a preset"); break;
            case Hit::SlotA:
            case Hit::SlotB: setTooltip ("Compare two settings: switching keeps the one you leave"); break;
            case Hit::Copy:  setTooltip ("Copy this setting into the other slot"); break;
            case Hit::None:  setTooltip ({}); break;
        }
        repaint();
    }
}

void PresetBar::mouseExit (const juce::MouseEvent&)
{
    hover_ = Hit::None;
    repaint();
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    pressed_ = hitAt (e.position);
    repaint();

    switch (pressed_)
    {
        case Hit::Prev:  presets_.loadNext (-1); break;
        case Hit::Next:  presets_.loadNext (+1); break;
        case Hit::Name:  showMenu(); break;
        case Hit::Save:  askToSave(); break;
        case Hit::SlotA: presets_.selectSlot (0); break;
        case Hit::SlotB: presets_.selectSlot (1); break;
        case Hit::Copy:  presets_.copyToOtherSlot(); break;
        case Hit::None:  break;
    }
    refresh();
}

void PresetBar::mouseUp (const juce::MouseEvent&)
{
    pressed_ = Hit::None;
    repaint();
}

void PresetBar::showMenu()
{
    const auto& list = presets_.refresh();

    juce::PopupMenu menu;
    menu.addSectionHeader ("FACTORY");
    bool anyUser = false;
    for (int i = 0; i < static_cast<int> (list.size()); ++i)
    {
        const auto& e = list[static_cast<std::size_t> (i)];
        if (! e.factory && ! anyUser)
        {
            menu.addSectionHeader ("YOURS");
            anyUser = true;
        }
        menu.addItem (i + 1, e.name, true, i == presets_.currentIndex());
    }
    if (! anyUser)
    {
        menu.addSectionHeader ("YOURS");
        menu.addItem (-1, "Nothing saved yet", false);
    }

    menu.addSeparator();
    menu.addItem (kSaveAs, "Save as...");
    const int cur = presets_.currentIndex();
    const bool canDelete = cur >= 0 && ! list[static_cast<std::size_t> (cur)].factory;
    menu.addItem (kDelete, canDelete ? "Delete \"" + presets_.currentName() + "\"" : juce::String ("Delete"), canDelete);
    menu.addItem (kShowFolder, "Show preset folder");

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (
                            localAreaToGlobal (area (Hit::Name).toNearestInt())),
                        [safe] (int id)
                        {
                            if (safe == nullptr || id == 0)
                                return;
                            auto& p = safe->presets_;
                            if (id == kSaveAs)
                                safe->askToSave();
                            else if (id == kDelete)
                                p.remove (p.currentIndex());
                            else if (id == kShowFolder)
                            {
                                PresetManager::defaultFolder().createDirectory();
                                PresetManager::defaultFolder().revealToUser();
                            }
                            else
                                p.load (id - 1);
                            safe->refresh();
                        });
}

void PresetBar::askToSave()
{
    auto* w = new juce::AlertWindow ("SAVE PRESET", "Name it. Saving over one of yours replaces it.",
                                     juce::MessageBoxIconType::NoIcon, this);
    const auto cur = presets_.currentIndex();
    const bool userPreset = cur >= 0 && ! presets_.entries()[static_cast<std::size_t> (cur)].factory;
    w->addTextEditor ("name", userPreset ? presets_.currentName() : juce::String());
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = w->getTextEditorContents ("name");
        if (! safe->presets_.save (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "NOT SAVED",
                                                    "Pick a name that isn't empty or a factory preset's.");
        safe->refresh();
    }), true);
}
