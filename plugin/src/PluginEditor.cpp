#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "PluginProcessor.h"
#include "Version.h"

namespace
{
// Design grid, in 1100 x 680 units.
constexpr float kMargin = 24.0f;
constexpr float kGap    = 16.0f;
constexpr float kChanW  = (1100.0f - 2.0f * kMargin - kGap) / 2.0f;
} // namespace

The89thEditor::ChannelUI::ChannelUI (The89thProcessor& p, int c)
    : display  (p.telemetry(), c, c == 0 ? "CH1" : "CH2"),
      delay    (p.apvts, pid::channel[c].delay,        "Delay",        "0",   "MAX"),
      pitch    (p.apvts, pid::channel[c].pitch,        "Pitch",        "-24", "+12"),
      fine     (p.apvts, pid::channel[c].fine,         "Fine"),
      xp1      (p.apvts, pid::channel[c].crosspoint1,  "Crosspoint 1", "0",   "MAX"),
      xp2      (p.apvts, pid::channel[c].crosspoint2,  "Crosspoint 2", "0",   "MAX"),
      feedback (p.apvts, pid::channel[c].feedback,     "Feedback",     "0",   "10"),
      vibDepth (p.apvts, pid::channel[c].vibratoDepth, "Depth",        "0",   "2"),
      vibRate  (p.apvts, pid::channel[c].vibratoRate,  "Speed",        "SLOW", "FAST"),
      vibShape (p.apvts, pid::channel[c].vibratoShape, "Shape",        { "SIN", "SQR" })
{
    for (auto* k : { &delay, &pitch, &fine, &xp1, &xp2, &feedback, &vibDepth, &vibRate })
        k->onTouch = [this] (const juce::String& legend, const juce::String& value)
        {
            display.showTouched (legend, value);
        };
}

std::vector<juce::Component*> The89thEditor::ChannelUI::controls()
{
    return { &delay, &pitch, &fine, &xp1, &xp2, &feedback, &vibDepth, &vibRate, &vibShape };
}

The89thEditor::The89thEditor (The89thProcessor& p)
    : AudioProcessorEditor (p),
      proc_ (p),
      presetBar_  (p.presets),
      mode_       (p.apvts, pid::mode,       "Mode",      { "DELAY", "PITCH" }),
      stereo_     (p.apvts, pid::stereo,     "Stereo",    { "TRUE", "QUASI" }),
      range_      (p.apvts, pid::range,      "Range",     { "LONG", "SHORT" }),
      bandwidth_  (p.apvts, pid::bandwidth,  "Bandwidth", { "5K", "10K", "20K" }),
      freeze_     (p.apvts, pid::freeze,     "Freeze", PushButton::Style::Accent),
      init_       (p.apvts, pid::init,       "Init",   PushButton::Style::Plain, true),
      mix_        (p.apvts, pid::mix,        "Mix",    "DRY", "WET"),
      route_      (p.apvts, pid::fbRoute,    "Routing",   { "NORM", "CROSS", "SUM" }),
      snap_       (p.apvts, pid::snap,       "Snap",      { "OFF", "CHR", "MAJ", "MIN", "PENT" }),
      scrubMode_  (p.apvts, pid::scrubMode,  "Mode",      { "LFO", "RND" }),
      lowCut_     (p.apvts, pid::lowCut,     "Low cut"),
      highCut_    (p.apvts, pid::highCut,    "High cut"),
      drive_      (p.apvts, pid::drive,      "Drive"),
      scrubDepth_ (p.apvts, pid::scrubDepth, "Depth"),
      scrubRate_  (p.apvts, pid::scrubRate,  "Speed"),
      sync_       (p.apvts, pid::sync,       "Sync",   PushButton::Style::Chip),
      link_       (p.apvts, pid::link,       "Link",   PushButton::Style::Chip),
      keys_       (p.apvts, pid::keys,       "Channels",  { "OFF", "L+R", "L", "R" }),
      keysRoot_   (p.apvts, pid::keysRoot,   "Root")
{
    setLookAndFeel (&lnf_);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar_, &mode_, &stereo_, &range_, &bandwidth_,
                                                             &freeze_, &init_, &mix_,
                                                             &route_, &lowCut_, &highCut_, &drive_,
                                                             &snap_, &sync_,
                                                             &scrubDepth_, &scrubRate_, &scrubMode_,
                                                             &link_, &keys_, &keysRoot_ })
        addAndMakeVisible (c);

    for (int c = 0; c < 2; ++c)
    {
        ch_[static_cast<std::size_t> (c)] = std::make_unique<ChannelUI> (p, c);
        auto& ui = *ch_[static_cast<std::size_t> (c)];
        addAndMakeVisible (ui.display);
        for (auto* k : ui.controls())
            addAndMakeVisible (k);
    }

    setResizable (true, true);
    setResizeLimits (kBaseW * 4 / 5, kBaseH * 4 / 5, kBaseW * 8 / 5, kBaseH * 8 / 5);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (kBaseW) / kBaseH);
    setSize (kBaseW, kBaseH);

    refresh();
    startTimerHz (30);
}

The89thEditor::~The89thEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void The89thEditor::refresh()
{
    const auto& t = proc_.telemetry();
    const bool traversal = ! t.delayMode.load() || t.frozen.load();
    const bool linked    = proc_.apvts.getRawParameterValue (pid::link)->load() > 0.5f;

    presetBar_.refresh();

    for (std::size_t c = 0; c < ch_.size(); ++c)
    {
        auto& ui = *ch_[c];
        ui.display.refresh();

        // Fade what the current mode ignores. The latch uses the crosspoints
        // and pitch even in delay mode. Linked, channel 2's own controls are
        // ignored altogether.
        const bool followed = linked && c == 1;
        for (auto* k : ui.controls())
            k->setAlpha (followed ? 0.25f : 1.0f);
        if (! followed)
        {
            ui.delay.setAlpha (traversal ? 0.35f : 1.0f);
            for (auto* k : std::initializer_list<juce::Component*> { &ui.pitch, &ui.fine, &ui.xp1, &ui.xp2 })
                k->setAlpha (traversal ? 1.0f : 0.35f);
        }

        // Played from the keyboard, a channel's Pitch knob is disconnected, as
        // the hardware's pot was by an external pitch clock. Fine still trims.
        if (t.voice[c].key.load() != -2)
            ui.pitch.setAlpha (0.35f);
    }
}

// ─── Painting ───────────────────────────────────────────────────────────────

void The89thEditor::section (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title) const
{
    const float s = static_cast<float> (getWidth()) / kBaseW;

    g.setColour (theme::hairline);
    g.drawRect (r, 1.0f);

    // Title in the top-left corner, cut into the line.
    const auto font = theme::mono (12.0f * s, true);
    const float tw  = juce::GlyphArrangement::getStringWidth (font, title) + 14.0f * s;
    const auto  box = juce::Rectangle<float> (r.getX() + 12.0f * s, r.getY() - 8.0f * s, tw, 16.0f * s);
    g.setColour (theme::bg);
    g.fillRect (box);
    g.setColour (theme::orange);
    g.setFont (font);
    g.drawText (title, box, juce::Justification::centred);
}

void The89thEditor::groupTitle (juce::Graphics& g, juce::Rectangle<float> span, const juce::String& title) const
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    const auto font = theme::mono (11.0f * s, true);
    const float tw  = juce::GlyphArrangement::getStringWidth (font, title) + 16.0f * s;
    const float y   = span.getCentreY();

    g.setColour (theme::hairline);
    g.drawLine (span.getX(), y, span.getCentreX() - tw * 0.5f, y, 1.0f);
    g.drawLine (span.getCentreX() + tw * 0.5f, y, span.getRight(), y, 1.0f);

    g.setColour (theme::text);
    g.setFont (font);
    g.drawText (title, span.withSizeKeepingCentre (tw, span.getHeight()), juce::Justification::centred);
}

void The89thEditor::paint (juce::Graphics& g)
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    auto R = [s] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * s, y * s, w * s, h * s); };

    g.fillAll (theme::bg);

    // Name and build stamp. The preset strip sits between them.
    g.setColour (theme::text);
    g.setFont (theme::mono (34.0f * s, true));
    g.drawText ("THE89TH", R (kMargin, 12, 200, 44), juce::Justification::centredLeft);

    {
        const auto plate = R (830, 18, 176, 34);
        g.setColour (theme::hairline);
        g.drawRect (plate, 1.0f);
        g.setColour (theme::textFaint);
        g.setFont (theme::mono (9.0f * s));
        g.drawText ("BUILD", plate.reduced (10.0f * s, 3.0f * s).withHeight (plate.getHeight() * 0.42f),
                    juce::Justification::centredLeft);
        g.setColour (theme::orange);
        g.setFont (theme::mono (11.0f * s, true));
        g.drawText (the89th_version::banner(), plate.reduced (10.0f * s, 0.0f).withTrimmedTop (plate.getHeight() * 0.40f),
                    juce::Justification::centredLeft);
    }

    // Rule under the header.
    g.setColour (theme::hairline);
    g.fillRect (R (kMargin, 64, 1100 - 2 * kMargin, 1));

    // The machine.
    section (g, R (kMargin, 84, 562, 94), "SYSTEM");
    section (g, R (kMargin + 574, 84, 136, 94), "LATCH");
    section (g, R (kMargin + 722, 84, 110, 94), "OUTPUT");
    section (g, R (kMargin + 844, 84, 208, 94), "KEYS");

    // Modern controls, all neutral by default.
    section (g, R (kMargin, 198, 460, 94), "FEEDBACK LOOP");
    section (g, R (kMargin + 476, 198, 280, 94), "MUSICAL");
    section (g, R (kMargin + 772, 198, 280, 94), "SCRUB");

    const juce::String names[2] = { "CHANNEL 1 - LEFT", "CHANNEL 2 - RIGHT" };
    for (int c = 0; c < 2; ++c)
    {
        const float x = kMargin + static_cast<float> (c) * (kChanW + kGap);
        section (g, R (x, 312, kChanW, 472), names[c]);

        groupTitle (g, R (x + 16, 536, kChanW - 32, 14), "READ");
        groupTitle (g, R (x + 16, 668, 150, 14), "RECIRCULATE");
        groupTitle (g, R (x + 190, 668, kChanW - 206, 14), "VIBRATO");
    }
}

void The89thEditor::resized()
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    presetBar_.setBounds (R (236, 20, 572, 30));
    init_.setBounds (R (1022, 14, 54, 46));

    mode_.setBounds      (R (kMargin + 14,  98, 118, 70));
    stereo_.setBounds    (R (kMargin + 142, 98, 118, 70));
    range_.setBounds     (R (kMargin + 270, 98, 118, 70));
    bandwidth_.setBounds (R (kMargin + 398, 98, 150, 70));
    freeze_.setBounds    (R (kMargin + 586, 106, 112, 52));
    mix_.setBounds       (R (kMargin + 730, 92, 94, 84));
    keys_.setBounds      (R (kMargin + 854, 98, 126, 70));
    keysRoot_.setBounds  (R (kMargin + 984, 92, 64, 84));

    // FEEDBACK LOOP
    route_.setBounds   (R (kMargin + 16,  212, 162, 70));
    lowCut_.setBounds  (R (kMargin + 192, 206, 84, 84));
    highCut_.setBounds (R (kMargin + 282, 206, 84, 84));
    drive_.setBounds   (R (kMargin + 372, 206, 84, 84));

    // MUSICAL
    snap_.setBounds (R (kMargin + 476 + 14, 212, 186, 70));
    sync_.setBounds (R (kMargin + 476 + 208, 240, 60, 28));

    // SCRUB
    scrubDepth_.setBounds (R (kMargin + 772 + 8,  206, 84, 84));
    scrubRate_.setBounds  (R (kMargin + 772 + 96, 206, 84, 84));
    scrubMode_.setBounds  (R (kMargin + 772 + 190, 212, 80, 70));

    for (int c = 0; c < 2; ++c)
    {
        const float x = kMargin + static_cast<float> (c) * (kChanW + kGap);
        auto& ui = *ch_[static_cast<std::size_t> (c)];

        ui.display.setBounds (R (x + 16, 328, kChanW - 32, 194));

        const float colW = (kChanW - 32) / 5.0f;
        Knob* row1[] = { &ui.delay, &ui.pitch, &ui.fine, &ui.xp1, &ui.xp2 };
        for (int i = 0; i < 5; ++i)
            row1[i]->setBounds (R (x + 16 + static_cast<float> (i) * colW + (colW - 86) * 0.5f, 552, 86, 108));

        ui.feedback.setBounds (R (x + 16 + 25, 684, 100, 94));
        ui.vibDepth.setBounds (R (x + 190, 684, 92, 94));
        ui.vibRate.setBounds  (R (x + 190 + 104, 684, 92, 94));
        ui.vibShape.setBounds (R (x + 190 + 218, 698, 84, 64));
    }

    // Link sits in channel 2's title line: it is channel 2 that follows.
    const float x2 = kMargin + kChanW + kGap;
    link_.setBounds (R (x2 + kChanW - 86, 301, 70, 22));
}
