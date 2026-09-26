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
      mode     (p.apvts, pid::channel[c].mode,         "Mode",         { "DELAY", "PITCH" }),
      latch    (p.apvts, pid::channel[c].freeze,       "Latch", PushButton::Style::Accent),
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
    return { &mode, &latch, &delay, &pitch, &fine, &xp1, &xp2, &feedback, &vibDepth, &vibRate, &vibShape };
}

The89thEditor::KeyboardUI::KeyboardUI (The89thProcessor& p)
    : trim       (p.apvts, pid::kbTrim,        "Trimmer",     "-", "+"),
      slope      (p.apvts, pid::kbSlope,       "Slope",       "0", "2S"),
      added      (p.apvts, pid::kbAdded,       "Added delay", "0", "MAX"),
      env        (p.apvts, pid::kbEnv,         "On",          PushButton::Style::Chip),
      attack     (p.apvts, pid::kbAttack,      "Attack"),
      hold       (p.apvts, pid::kbHold,        "Hold"),
      release    (p.apvts, pid::kbRelease,     "Release"),
      vib        (p.apvts, pid::kbVib,         "On",          PushButton::Style::Chip),
      vibRate    (p.apvts, pid::kbVibRate,     "Frequency"),
      vibSharp   (p.apvts, pid::kbVibSharp,    "Sharpness"),
      vibDepth   (p.apvts, pid::kbVibDepth,    "Depth"),
      modRate    (p.apvts, pid::kbVibModRate,  "Mod freq",    "-", "+"),
      modSharp   (p.apvts, pid::kbVibModSharp, "Mod sharp",   "-", "+"),
      modDepth   (p.apvts, pid::kbVibModDepth, "Mod depth",   "-", "+"),
      vibAttack  (p.apvts, pid::kbVibAttack,   "Mod attack"),
      vibRelease (p.apvts, pid::kbVibRelease,  "Mod release"),
      synchro    (p.apvts, pid::kbSynchro,     "Sides",       { "OFF", "L", "R", "BOTH" }),
      attackPt   (p.apvts, pid::kbAttackPt,    "Attack pt"),
      returnPt   (p.apvts, pid::kbReturnPt,    "Return pt"),
      endPt      (p.apvts, pid::kbEndPt,       "End pt"),
      speed      (p.apvts, pid::kbSpeed,       "Speed",       "FREE", "2X"),
      reverse    (p.apvts, pid::kbReverse,     "Sides",       { "OFF", "L", "R", "BOTH" }),
      gate       (p.apvts, pid::kbGate,        "Gate",        PushButton::Style::Chip),
      thresh     (p.apvts, pid::kbThresh,      "Threshold"),
      revDelay   (p.apvts, pid::kbRevDelay,    "Delay",       "0", "1S")
{
}

std::vector<juce::Component*> The89thEditor::KeyboardUI::controls()
{
    return { &trim, &slope, &added, &env, &attack, &hold, &release,
             &vib, &vibRate, &vibSharp, &vibDepth, &modRate, &modSharp, &modDepth, &vibAttack, &vibRelease,
             &synchro, &attackPt, &returnPt, &endPt, &speed,
             &reverse, &gate, &thresh, &revDelay };
}

juce::Rectangle<float> The89thEditor::synchroBar()
{
    return { kMargin + 426 + 212, 574, 124, 56 };
}

The89thEditor::The89thEditor (The89thProcessor& p)
    : AudioProcessorEditor (p),
      proc_ (p),
      presetBar_  (p.presets),
      stereo_     (p.apvts, pid::stereo,     "Stereo",    { "TRUE", "QUASI" }),
      range_      (p.apvts, pid::range,      "Range",     { "LONG", "SHORT" }),
      bandwidth_  (p.apvts, pid::bandwidth,  "Bandwidth", { "5K", "10K", "20K" }),
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
      keys_       (p.apvts, pid::keys,       "Channels",  { "OFF", "L", "R", "BI" }),
      kbPlay_     (p.apvts, pid::kbPlay,     "Play",      { "PUSH", "SUST" }),
      keysRoot_   (p.apvts, pid::keysRoot,   "Root"),
      kb_         (p)
{
    setLookAndFeel (&lnf_);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar_, &stereo_, &range_, &bandwidth_,
                                                             &init_, &mix_,
                                                             &route_, &lowCut_, &highCut_, &drive_,
                                                             &snap_, &sync_,
                                                             &scrubDepth_, &scrubRate_, &scrubMode_,
                                                             &link_, &keys_, &kbPlay_, &keysRoot_, &kbPage_ })
        addAndMakeVisible (c);

    for (auto* c : kb_.controls())
        addChildComponent (c);
    kbPage_.setTooltip ("Show the original keyboard's panel in place of the channels");
    kbPage_.onChange = [this] (bool on) { showKeyboardPage (on); };

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

void The89thEditor::showKeyboardPage (bool on)
{
    kbPage_.setOn (on);
    for (auto* c : kb_.controls())
        c->setVisible (on);
    for (auto& ui : ch_)
    {
        ui->display.setVisible (! on);
        for (auto* c : ui->controls())
            c->setVisible (! on);
    }
    link_.setVisible (! on);
    repaint();
}

The89thEditor::~The89thEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void The89thEditor::refresh()
{
    const auto& t = proc_.telemetry();
    const bool linked    = proc_.apvts.getRawParameterValue (pid::link)->load() > 0.5f;

    presetBar_.refresh();

    for (std::size_t c = 0; c < ch_.size(); ++c)
    {
        auto& ui = *ch_[c];
        ui.display.refresh();
        const bool traversal = ! t.voice[c].delayMode.load() || t.voice[c].frozen.load();

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
        {
            ui.pitch.setAlpha (0.35f);
            ui.fine.setAlpha (0.35f);
        }
    }

    if (kbPage_.isOn())
    {
        // Each section fades while its switch is off.
        auto choice = [this] (const char* id) { return proc_.apvts.getRawParameterValue (id)->load() > 0.5f; };
        const bool keys = choice (pid::keys);
        for (auto* k : std::initializer_list<juce::Component*> { &kb_.trim, &kb_.slope, &kb_.added })
            k->setAlpha (keys ? 1.0f : 0.35f);
        for (auto* k : std::initializer_list<juce::Component*> { &kb_.attack, &kb_.hold, &kb_.release })
            k->setAlpha (keys && choice (pid::kbEnv) ? 1.0f : 0.35f);
        for (auto* k : std::initializer_list<juce::Component*> { &kb_.vibRate, &kb_.vibSharp, &kb_.vibDepth, &kb_.modRate,
                                                                 &kb_.modSharp, &kb_.modDepth, &kb_.vibAttack, &kb_.vibRelease })
            k->setAlpha (keys && choice (pid::kbVib) ? 1.0f : 0.35f);
        for (auto* k : std::initializer_list<juce::Component*> { &kb_.attackPt, &kb_.returnPt, &kb_.endPt, &kb_.speed })
            k->setAlpha (keys && choice (pid::kbSynchro) ? 1.0f : 0.35f);
        for (auto* k : std::initializer_list<juce::Component*> { &kb_.thresh, &kb_.revDelay })
            k->setAlpha (choice (pid::kbReverse) ? 1.0f : 0.35f);

        const float s = static_cast<float> (getWidth()) / kBaseW;
        const auto bar = synchroBar();
        repaint (juce::Rectangle<float> (bar.getX() * s, bar.getY() * s, bar.getWidth() * s, bar.getHeight() * s)
                     .toNearestInt().expanded (2));
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
    section (g, R (kMargin, 84, 434, 94), "SYSTEM");
    section (g, R (kMargin + 450, 84, 110, 94), "OUTPUT");
    section (g, R (kMargin + 576, 84, 476, 94), "KEYS");

    // Modern controls, all neutral by default.
    section (g, R (kMargin, 198, 460, 94), "FEEDBACK LOOP");
    section (g, R (kMargin + 476, 198, 280, 94), "MUSICAL");
    section (g, R (kMargin + 772, 198, 280, 94), "SCRUB");

    if (kbPage_.isOn())
    {
        section (g, R (kMargin, 312, 518, 220), "KEYBOARD - PITCH RATIO");
        section (g, R (kMargin + 534, 312, 518, 220), "ENVELOPE");
        section (g, R (kMargin, 552, 410, 232), "VIBRATO");
        section (g, R (kMargin + 426, 552, 356, 232), "MEMORY SYNCHRO");
        section (g, R (kMargin + 798, 552, 254, 232), "REVERSE SYNCHRO");

        g.setColour (theme::textFaint);
        g.setFont (theme::mono (10.0f * s));
        g.drawText ("KEYS REPLACE THE PITCH POTS OF THE SIDES THEY PLAY", R (kMargin + 16, 500, 486, 16),
                    juce::Justification::centred);
        g.drawText ("FOR LIVE INPUT ONLY", R (kMargin + 798 + 16, 760, 222, 14), juce::Justification::centred);

        // The panel's "instantaneous display of memory reading position": a
        // row of lights per side, one lit where each side is reading.
        const auto bar = synchroBar();
        const auto& t  = proc_.telemetry();
        constexpr int kLights = 30;
        for (int c = 0; c < 2; ++c)
        {
            const float pos = t.voice[static_cast<std::size_t> (c)].synchroPos.load();
            const int   lit = pos < 0.0f ? -1 : juce::jlimit (0, kLights - 1, static_cast<int> (pos * kLights));
            const float y   = bar.getY() + 14.0f + static_cast<float> (c) * 22.0f;
            g.setColour (theme::textDim);
            g.setFont (theme::mono (9.0f * s, true));
            g.drawText (c == 0 ? "L" : "R", R (bar.getX(), y - 6.0f, 10, 12), juce::Justification::centredLeft);
            for (int i = 0; i < kLights; ++i)
                draw::dot (g, { (bar.getX() + 14.0f + static_cast<float> (i) * 3.7f) * s, y * s }, 1.4f * s, i == lit);
        }
        return;
    }

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

    stereo_.setBounds    (R (kMargin + 14,  98, 118, 70));
    range_.setBounds     (R (kMargin + 142, 98, 118, 70));
    bandwidth_.setBounds (R (kMargin + 270, 98, 150, 70));
    mix_.setBounds       (R (kMargin + 458, 92, 94, 84));
    keys_.setBounds      (R (kMargin + 590, 98, 150, 70));
    kbPlay_.setBounds    (R (kMargin + 754, 98, 100, 70));
    keysRoot_.setBounds  (R (kMargin + 864, 92, 64, 84));
    kbPage_.setBounds    (R (kMargin + 944, 118, 96, 26));

    // The keyboard page.
    {
        auto knobRow = [&R] (std::initializer_list<Knob*> knobs, float x, float w, float y, float kw, float kh)
        {
            const float col = w / static_cast<float> (knobs.size());
            float cx = x;
            for (auto* k : knobs)
            {
                k->setBounds (R (cx + (col - kw) * 0.5f, y, kw, kh));
                cx += col;
            }
        };
        knobRow ({ &kb_.trim, &kb_.slope, &kb_.added }, kMargin + 16, 486, 344, 100, 120);
        kb_.env.setBounds (R (kMargin + 534 + 518 - 86, 301, 70, 22));
        knobRow ({ &kb_.attack, &kb_.hold, &kb_.release }, kMargin + 534 + 16, 486, 344, 100, 120);

        kb_.vib.setBounds (R (kMargin + 410 - 86, 541, 70, 22));
        knobRow ({ &kb_.vibRate, &kb_.vibSharp, &kb_.vibDepth, &kb_.vibAttack }, kMargin + 16, 378, 570, 84, 98);
        knobRow ({ &kb_.modRate, &kb_.modSharp, &kb_.modDepth, &kb_.vibRelease }, kMargin + 16, 378, 676, 84, 98);

        kb_.synchro.setBounds (R (kMargin + 426 + 14, 568, 186, 64));
        knobRow ({ &kb_.attackPt, &kb_.returnPt, &kb_.endPt, &kb_.speed }, kMargin + 426 + 10, 336, 650, 80, 108);

        kb_.reverse.setBounds (R (kMargin + 798 + 14, 568, 160, 64));
        kb_.gate.setBounds    (R (kMargin + 798 + 182, 592, 58, 26));
        knobRow ({ &kb_.thresh, &kb_.revDelay }, kMargin + 798 + 10, 234, 640, 96, 112);
    }

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

        // Mode and latch sit at the panel's outer edges, as on the hardware:
        // channel 1's on the left, channel 2's on the right.
        constexpr float kSideW = 104.0f;
        const float side = c == 0 ? x + 16 : x + kChanW - 16 - kSideW;
        const float disp = c == 0 ? x + 16 + kSideW + 12 : x + 16;
        ui.mode.setBounds    (R (side, 340, kSideW, 70));
        ui.latch.setBounds   (R (side, 432, kSideW, 52));
        ui.display.setBounds (R (disp, 328, kChanW - 44 - kSideW, 194));

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
