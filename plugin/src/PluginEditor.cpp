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
      xp1      (p.apvts, pid::channel[c].crosspoint1,  "Crosspoint 1", "0",   "MAX"),
      xp2      (p.apvts, pid::channel[c].crosspoint2,  "Crosspoint 2", "0",   "MAX"),
      feedback (p.apvts, pid::channel[c].feedback,     "Feedback",     "0",   "10"),
      vibDepth (p.apvts, pid::channel[c].vibratoDepth, "Depth",        "0",   "2"),
      vibRate  (p.apvts, pid::channel[c].vibratoRate,  "Speed",        "SLOW", "FAST")
{
    for (auto* k : { &delay, &pitch, &xp1, &xp2, &feedback, &vibDepth, &vibRate })
        k->onTouch = [this] (const juce::String& legend, const juce::String& value)
        {
            display.showTouched (legend, value);
        };
}

The89thEditor::The89thEditor (The89thProcessor& p)
    : AudioProcessorEditor (p),
      proc_ (p),
      mode_      (p.apvts, pid::mode,      "Mode",      { "DELAY", "PITCH" }),
      stereo_    (p.apvts, pid::stereo,    "Stereo",    { "TRUE", "QUASI" }),
      range_     (p.apvts, pid::range,     "Range",     { "LONG", "SHORT" }),
      bandwidth_ (p.apvts, pid::bandwidth, "Bandwidth", { "5K", "10K", "20K" }),
      freeze_    (p.apvts, pid::freeze,    "Freeze", PushButton::Style::Accent),
      init_      (p.apvts, pid::init,      "Init",   PushButton::Style::Plain, true),
      mix_       (p.apvts, pid::mix,       "Mix",    "DRY", "WET")
{
    setLookAndFeel (&lnf_);

    for (auto* c : std::initializer_list<juce::Component*> { &mode_, &stereo_, &range_, &bandwidth_,
                                                             &freeze_, &init_, &mix_ })
        addAndMakeVisible (c);

    for (int c = 0; c < 2; ++c)
    {
        ch_[static_cast<std::size_t> (c)] = std::make_unique<ChannelUI> (p, c);
        auto& ui = *ch_[static_cast<std::size_t> (c)];
        for (auto* k : std::initializer_list<juce::Component*> { &ui.display, &ui.delay, &ui.pitch, &ui.xp1, &ui.xp2,
                                                                 &ui.feedback, &ui.vibDepth, &ui.vibRate })
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

    for (auto& ui : ch_)
    {
        ui->display.refresh();

        // Fade what the current mode ignores. The latch uses the crosspoints
        // and pitch even in delay mode.
        ui->delay.setAlpha (traversal ? 0.35f : 1.0f);
        for (auto* k : { &ui->pitch, &ui->xp1, &ui->xp2 })
            k->setAlpha (traversal ? 1.0f : 0.35f);
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

    // Name, description, build stamp.
    g.setColour (theme::text);
    g.setFont (theme::mono (34.0f * s, true));
    g.drawText ("THE89TH", R (kMargin, 12, 200, 44), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (theme::mono (10.5f * s));
    g.drawText ("DUAL CHANNEL DIGITAL PITCH TRANSPOSER / DELAY", R (232, 20, 520, 14), juce::Justification::centredLeft);
    g.drawText ("TRUE AND QUASI STEREO - 16384 WORD MEMORY - FLYING COMMA CONVERTER", R (232, 36, 560, 14),
                juce::Justification::centredLeft);

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

    // Sections.
    section (g, R (kMargin, 84, 700, 94), "SYSTEM");
    section (g, R (kMargin + 716, 84, 170, 94), "LATCH");
    section (g, R (kMargin + 902, 84, 150, 94), "OUTPUT");

    const juce::String names[2] = { "CHANNEL 1 - LEFT", "CHANNEL 2 - RIGHT" };
    for (int c = 0; c < 2; ++c)
    {
        const float x = kMargin + static_cast<float> (c) * (kChanW + kGap);
        section (g, R (x, 198, kChanW, 466), names[c]);

        groupTitle (g, R (x + 16, 420, kChanW - 32, 14), "READ");
        groupTitle (g, R (x + 16, 552, 150, 14), "RECIRCULATE");
        groupTitle (g, R (x + 190, 552, kChanW - 206, 14), "VIBRATO");
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

    init_.setBounds (R (1022, 14, 54, 46));

    mode_.setBounds      (R (kMargin + 18,  98, 150, 70));
    stereo_.setBounds    (R (kMargin + 190, 98, 150, 70));
    range_.setBounds     (R (kMargin + 362, 98, 150, 70));
    bandwidth_.setBounds (R (kMargin + 534, 98, 150, 70));
    freeze_.setBounds    (R (kMargin + 736, 106, 130, 52));
    mix_.setBounds       (R (kMargin + 928, 92, 98, 84));

    for (int c = 0; c < 2; ++c)
    {
        const float x = kMargin + static_cast<float> (c) * (kChanW + kGap);
        auto& ui = *ch_[static_cast<std::size_t> (c)];

        ui.display.setBounds (R (x + 16, 214, kChanW - 32, 194));

        const float colW = (kChanW - 32) / 4.0f;
        Knob* row1[] = { &ui.delay, &ui.pitch, &ui.xp1, &ui.xp2 };
        for (int i = 0; i < 4; ++i)
            row1[i]->setBounds (R (x + 16 + static_cast<float> (i) * colW + (colW - 100) * 0.5f, 434, 100, 116));

        ui.feedback.setBounds (R (x + 16 + 25, 566, 100, 94));
        ui.vibDepth.setBounds (R (x + 190 + 40, 566, 100, 94));
        ui.vibRate.setBounds  (R (x + 190 + 160, 566, 100, 94));
    }
}
