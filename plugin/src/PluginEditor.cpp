#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "PluginProcessor.h"
#include "Version.h"

namespace
{
// Design grid, in 1100 x 680 units.
constexpr float kCheek  = 30.0f;
constexpr float kMargin = 44.0f;
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
      freeze_    (p.apvts, pid::freeze,    "Freeze", PushButton::Style::Cream),
      init_      (p.apvts, pid::init,      "Init",   PushButton::Style::Dark, true),
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

    g.setColour (theme::rule);
    g.drawRoundedRectangle (r, 4.0f * s, 1.0f * s);

    // Title set into the top line: knock out the rule behind it, then print.
    const auto font = theme::title (15.0f * s);
    const float tw  = juce::GlyphArrangement::getStringWidth (font, title) + 12.0f * s;
    const auto  box = juce::Rectangle<float> (r.getX() + 14.0f * s, r.getY() - 8.0f * s, tw, 16.0f * s);
    g.setColour (theme::panel);
    g.fillRect (box);
    g.setColour (theme::print);
    g.setFont (font);
    g.drawText (title, box, juce::Justification::centred);
}

void The89thEditor::groupTitle (juce::Graphics& g, juce::Rectangle<float> span, const juce::String& title) const
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    const auto font = theme::title (13.0f * s);
    const float tw  = juce::GlyphArrangement::getStringWidth (font, title) + 14.0f * s;
    const float y   = span.getCentreY();

    g.setColour (theme::rule.withMultipliedAlpha (0.7f));
    g.drawLine (span.getX(), y, span.getCentreX() - tw * 0.5f, y, 1.0f * s);
    g.drawLine (span.getCentreX() + tw * 0.5f, y, span.getRight(), y, 1.0f * s);
    // Short end ticks, as silkscreened group brackets have.
    g.drawLine (span.getX(), y, span.getX(), y + 5.0f * s, 1.0f * s);
    g.drawLine (span.getRight(), y, span.getRight(), y + 5.0f * s, 1.0f * s);

    g.setColour (theme::print);
    g.setFont (font);
    g.drawText (title, span.withSizeKeepingCentre (tw, span.getHeight()), juce::Justification::centred);
}

void The89thEditor::rebuildWood()
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    const int w = juce::jmax (1, juce::roundToInt (kCheek * s));
    const int h = juce::jmax (1, getHeight());

    wood_ = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (wood_);

    // Base: warm brown, darker toward both edges where the timber rounds off.
    juce::ColourGradient base (theme::woodDark, 0.0f, 0.0f, theme::woodDark, static_cast<float> (w), 0.0f, false);
    base.addColour (0.35, theme::wood);
    base.addColour (0.65, theme::wood.brighter (0.08f));
    g.setGradientFill (base);
    g.fillAll();

    // Grain: long, gently wandering lines, fixed seed so it never shimmers
    // between repaints or sessions.
    juce::Random rng (8989);
    for (int i = 0; i < 90; ++i)
    {
        const float x0    = rng.nextFloat() * static_cast<float> (w);
        const float amp   = (0.4f + rng.nextFloat() * 1.6f) * s;
        const float freq  = 0.004f + rng.nextFloat() * 0.01f;
        const float phase = rng.nextFloat() * 6.283f;
        const bool  dark  = rng.nextFloat() < 0.75f;

        juce::Path grain;
        grain.startNewSubPath (x0, 0.0f);
        for (float y = 0.0f; y <= static_cast<float> (h); y += 6.0f)
            grain.lineTo (x0 + amp * std::sin (y * freq + phase) + amp * 0.3f * std::sin (y * freq * 3.1f), y);

        g.setColour (dark ? theme::woodDark.withAlpha (0.12f + rng.nextFloat() * 0.3f)
                          : juce::Colour (0xffa07850).withAlpha (0.05f + rng.nextFloat() * 0.12f));
        g.strokePath (grain, juce::PathStrokeType ((0.4f + rng.nextFloat() * 1.2f) * s));
    }

    // Lit front edge on the outside, and a varnish sheen.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (0, 0, juce::jmax (1, w / 6), h);
}

void The89thEditor::paint (juce::Graphics& g)
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    auto R = [s] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * s, y * s, w * s, h * s); };

    // Panel, then the cheeks either side with a shadow where they meet it.
    g.fillAll (theme::panel);
    if (wood_.isValid())
    {
        g.drawImageAt (wood_, 0, 0);
        g.drawImageTransformed (wood_, juce::AffineTransform::scale (-1.0f, 1.0f)
                                           .translated (static_cast<float> (getWidth()), 0.0f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (R (kCheek, 0, 2, kBaseH));
    g.fillRect (R (kBaseW - kCheek - 2, 0, 2, kBaseH));

    // Corner screws.
    for (auto p : { juce::Point<float> (46, 16), juce::Point<float> (1054, 16),
                    juce::Point<float> (46, 664), juce::Point<float> (1054, 664) })
    {
        const auto c = p * s;
        const float r = 5.0f * s;
        juce::ColourGradient grad (juce::Colour (0xff5c5c60), c.x, c.y - r, juce::Colour (0xff1c1c1e), c.x, c.y + r, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff0d0d0e));
        g.drawLine (c.x - r * 0.7f, c.y + r * 0.2f, c.x + r * 0.7f, c.y - r * 0.2f, 1.2f * s);
    }

    // Name and description.
    g.setColour (theme::print);
    g.setFont (theme::title (40.0f * s));
    g.drawText ("THE89TH", R (66, 14, 190, 42), juce::Justification::centredLeft);

    g.setColour (theme::printDim);
    g.setFont (theme::legend (11.0f * s));
    g.drawText ("DUAL CHANNEL DIGITAL PITCH TRANSPOSER  /  DELAY", R (222, 24, 400, 14), juce::Justification::centredLeft);
    g.drawText ("TRUE AND QUASI STEREO  -  16384 WORD MEMORY  -  FLYING COMMA CONVERTER",
                R (222, 38, 480, 14), juce::Justification::centredLeft);

    // Serial plate, carrying the build stamp.
    {
        const auto plate = R (812, 20, 150, 30);
        g.setColour (juce::Colour (0xff1b1b1d));
        g.fillRoundedRectangle (plate, 2.0f * s);
        g.setColour (theme::rule.withMultipliedAlpha (0.5f));
        g.drawRoundedRectangle (plate, 2.0f * s, 1.0f * s);
        g.setColour (theme::printDim);
        g.setFont (theme::legend (8.5f * s));
        g.drawText ("SERIAL", plate.withTrimmedLeft (8.0f * s).withHeight (plate.getHeight() * 0.45f).translated (0, 2.0f * s),
                    juce::Justification::centredLeft);
        g.setColour (theme::print);
        g.setFont (theme::screen (10.0f * s));
        g.drawText (the89th_version::banner(), plate.withTrimmedLeft (8.0f * s).withTrimmedTop (plate.getHeight() * 0.42f),
                    juce::Justification::centredLeft);
    }

    // Sections.
    section (g, R (kMargin, 74, 690, 104), "SYSTEM");
    section (g, R (748, 74, 150, 104), "LATCH");
    section (g, R (912, 74, 144, 104), "OUTPUT");

    const juce::String names[2] = { "CHANNEL 1  -  LEFT", "CHANNEL 2  -  RIGHT" };
    for (int c = 0; c < 2; ++c)
    {
        const float x = c == 0 ? kMargin : 558.0f;
        section (g, R (x, 194, 498, 470), names[c]);

        groupTitle (g, R (x + 16, 416, 466, 14), "READ");
        groupTitle (g, R (x + 16, 546, 150, 14), "RECIRCULATE");
        groupTitle (g, R (x + 190, 546, 292, 14), "VIBRATO");
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

    init_.setBounds (R (990, 12, 52, 50));

    mode_.setBounds      (R (62,  90, 150, 80));
    stereo_.setBounds    (R (226, 90, 150, 80));
    range_.setBounds     (R (390, 90, 150, 80));
    bandwidth_.setBounds (R (554, 90, 170, 80));
    freeze_.setBounds    (R (772, 88, 102, 84));
    mix_.setBounds       (R (938, 86, 92, 88));

    for (int c = 0; c < 2; ++c)
    {
        const float x = c == 0 ? kMargin : 558.0f;
        auto& ui = *ch_[static_cast<std::size_t> (c)];

        ui.display.setBounds (R (x + 16, 210, 466, 196));

        Knob* row1[] = { &ui.delay, &ui.pitch, &ui.xp1, &ui.xp2 };
        for (int i = 0; i < 4; ++i)
            row1[i]->setBounds (R (x + 16 + i * 116.5f + 10, 432, 96, 108));

        ui.feedback.setBounds (R (x + 16 + 27, 562, 96, 96));
        ui.vibDepth.setBounds (R (x + 190 + 40, 562, 96, 96));
        ui.vibRate.setBounds  (R (x + 190 + 156, 562, 96, 96));
    }

    rebuildWood();
}
