#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "PluginProcessor.h"
#include "Version.h"

The89thEditor::ChannelUI::ChannelUI (The89thProcessor& p, int c)
    : ring     (p.telemetry(), c, c == 0 ? "LEFT" : "RIGHT"),
      delay    (p.apvts, pid::channel[c].delay,        "Delay"),
      pitch    (p.apvts, pid::channel[c].pitch,        "Pitch", true),
      xp1      (p.apvts, pid::channel[c].crosspoint1,  "Crosspoint 1"),
      xp2      (p.apvts, pid::channel[c].crosspoint2,  "Crosspoint 2"),
      feedback (p.apvts, pid::channel[c].feedback,     "Feedback"),
      vibDepth (p.apvts, pid::channel[c].vibratoDepth, "Vibrato"),
      vibRate  (p.apvts, pid::channel[c].vibratoRate,  "Speed")
{
}

The89thEditor::The89thEditor (The89thProcessor& p)
    : AudioProcessorEditor (p),
      proc_ (p),
      mode_      (p.apvts, pid::mode,      "Mode",      { "Delay", "Pitch" }),
      stereo_    (p.apvts, pid::stereo,    "Stereo",    { "True", "Quasi" }),
      range_     (p.apvts, pid::range,     "Range",     { "Long", "Short" }),
      bandwidth_ (p.apvts, pid::bandwidth, "Bandwidth", { "5k", "10k", "20k" }),
      freeze_    (p.apvts, pid::freeze,    "Freeze"),
      init_      (p.apvts, pid::init,      "Init", true),
      mix_       (p.apvts, pid::mix,       "Mix")
{
    setLookAndFeel (&lnf_);

    for (auto* c : std::initializer_list<juce::Component*> { &mode_, &stereo_, &range_, &bandwidth_,
                                                             &freeze_, &init_, &mix_ })
        addAndMakeVisible (c);

    for (int c = 0; c < 2; ++c)
    {
        ch_[static_cast<std::size_t> (c)] = std::make_unique<ChannelUI> (p, c);
        auto& ui = *ch_[static_cast<std::size_t> (c)];
        for (auto* k : std::initializer_list<juce::Component*> { &ui.ring, &ui.delay, &ui.pitch, &ui.xp1, &ui.xp2,
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
    const bool delayMode = t.delayMode.load();
    const bool frozen    = t.frozen.load();
    const bool traversal = ! delayMode || frozen;

    for (auto& ui : ch_)
    {
        ui->ring.refresh();

        // Fade what the current mode ignores. The latch uses the crosspoints
        // and pitch even in delay mode.
        ui->delay.setAlpha (traversal ? 0.4f : 1.0f);
        for (auto* k : { &ui->pitch, &ui->xp1, &ui->xp2 })
            k->setAlpha (traversal ? 1.0f : 0.4f);
    }
}

void The89thEditor::paint (juce::Graphics& g)
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    g.fillAll (theme::bg);

    // Header: name, description, build stamp.
    auto h = header_.toFloat();
    g.setColour (theme::text);
    g.setFont (theme::font (28.0f * s, true).withExtraKerningFactor (0.18f));
    g.drawText ("THE89TH", h.removeFromLeft (190.0f * s), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (theme::font (13.0f * s));
    g.drawText ("dual-channel pitch-shifting delay", h.removeFromLeft (320.0f * s),
                juce::Justification::centredLeft);

    g.setColour (theme::textFaint);
    g.setFont (theme::mono (11.0f * s));
    g.drawText (the89th_version::banner(),
                h.withTrimmedRight (init_.getWidth() + 16.0f * s), juce::Justification::centredRight);

    // Rule under the header, and the switch strip's panel.
    g.setColour (theme::line);
    g.fillRect (juce::Rectangle<float> (header_.getX(), header_.getBottom() + 6.0f * s,
                                        static_cast<float> (header_.getWidth()), 1.0f));

    g.setColour (theme::panel);
    g.fillRoundedRectangle (strip_.toFloat(), 10.0f * s);

    // Channel panels. The rings paint their own upper part.
    for (const auto& r : channelPanels_)
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f * s);
    }
}

void The89thEditor::resized()
{
    const float s = static_cast<float> (getWidth()) / kBaseW;
    auto S = [s] (float v) { return juce::roundToInt (v * s); };
    auto R = [&] (float x, float y, float w, float hh) { return juce::Rectangle<int> (S (x), S (y), S (w), S (hh)); };

    header_ = R (24, 12, 1052, 44);
    init_.setBounds (R (996, 20, 80, 28));

    strip_ = R (18, 68, 1064, 86);
    mode_.setBounds      (R (40,  84, 170, 54));
    stereo_.setBounds    (R (232, 84, 170, 54));
    range_.setBounds     (R (424, 84, 150, 54));
    bandwidth_.setBounds (R (596, 84, 190, 54));
    freeze_.setBounds    (R (818, 95, 140, 44));
    mix_.setBounds       (R (982, 70, 92, 84));

    const float panelW = 523.0f, panelY = 164.0f, panelH = 500.0f;
    for (int c = 0; c < 2; ++c)
    {
        const float x = 18.0f + c * (panelW + 18.0f);
        channelPanels_[static_cast<std::size_t> (c)] = R (x, panelY, panelW, panelH);

        auto& ui = *ch_[static_cast<std::size_t> (c)];
        ui.ring.setBounds (R (x, panelY, panelW, 294));

        // Row one: where it reads from. Row two: what it does to it.
        const float k1 = 96.0f, k1h = 104.0f, gap1 = (panelW - 4 * k1) / 5.0f;
        Knob* row1[] = { &ui.delay, &ui.pitch, &ui.xp1, &ui.xp2 };
        for (int i = 0; i < 4; ++i)
            row1[i]->setBounds (R (x + gap1 + i * (k1 + gap1), panelY + 294, k1, k1h));

        const float k2 = 84.0f, k2h = 92.0f, gap2 = (panelW - 3 * k2) / 4.0f;
        Knob* row2[] = { &ui.feedback, &ui.vibDepth, &ui.vibRate };
        for (int i = 0; i < 3; ++i)
            row2[i]->setBounds (R (x + gap2 + i * (k2 + gap2), panelY + 400, k2, k2h));
    }
}
