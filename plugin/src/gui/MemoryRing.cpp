#include "MemoryRing.h"
#include "Theme.h"

#include <cmath>

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

juce::String ms (float v)
{
    return v < 100.0f ? juce::String (v, 1) + "MS" : juce::String (juce::roundToInt (v)) + "MS";
}
} // namespace

MemoryRing::MemoryRing (const Telemetry& t, int channel, juce::String title)
    : t_ (t), channel_ (channel), title_ (std::move (title))
{
    setInterceptsMouseClicks (false, false);
}

void MemoryRing::showTouched (const juce::String& legend, const juce::String& value)
{
    touchedText_  = legend + "  " + value.toUpperCase().removeCharacters (" ");
    touchedTicks_ = 45;   // about a second and a half at the editor's 30 Hz
    repaint();
}

void MemoryRing::refresh()
{
    const auto  ci = static_cast<std::size_t> (channel_);
    const auto& tv = t_.voice[ci];

    v_.writePos  = t_.writePos[ci].load();
    v_.primary   = tv.primary.load();
    v_.secondary = tv.secondary.load();
    v_.gainA     = tv.gainA.load();
    v_.gainB     = tv.gainB.load();
    v_.lo        = tv.regionLo.load();
    v_.hi        = tv.regionHi.load();
    v_.match     = tv.match.load();
    v_.rate      = tv.rate.load();
    v_.delayMs   = tv.delayMs.load();
    v_.traversal = tv.traversal.load();
    v_.splicing  = tv.splicing.load();
    v_.reversed  = tv.reversed.load();
    v_.words     = std::max (1, t_.words.load());
    v_.msPerWord = t_.msPerWord.load();
    v_.frozen    = t_.frozen.load();
    v_.delayMode = t_.delayMode.load();
    v_.quasi     = t_.quasi.load();

    const float peak = tv.peak.load();
    v_.level = peak > v_.level ? peak : v_.level * 0.86f;

    spliceFlash_ = (v_.splicing && ! wasSplicing_) ? 1.0f : spliceFlash_ * 0.8f;
    wasSplicing_ = v_.splicing;

    if (touchedTicks_ > 0)
        --touchedTicks_;

    repaint();
}

float MemoryRing::angleOf (float delayWords) const
{
    float frac = v_.writePos - delayWords / static_cast<float> (v_.words);
    frac -= std::floor (frac);
    return frac * kTwoPi;
}

void MemoryRing::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float u = b.getHeight() / 200.0f;   // design unit

    // Bezel and glass.
    g.setColour (juce::Colours::black);
    g.fillRect (b);
    g.setColour (juce::Colour (0xff2b2b2e));
    g.drawRect (b, 1.0f);
    auto glass = b.reduced (5.0f * u);
    g.setColour (theme::oledBg);
    g.fillRect (glass);

    auto text = [&] (const juce::String& s, juce::Rectangle<float> area, juce::Justification j,
                     juce::Colour col, float size)
    {
        g.setColour (col);
        g.setFont (theme::screen (size * u));
        g.drawText (s, area, j);
    };

    auto inner  = glass.reduced (8.0f * u, 6.0f * u);
    auto top    = inner.removeFromTop (14.0f * u);
    auto bottom = inner.removeFromBottom (14.0f * u);

    const bool  pitchPath = v_.traversal;
    const float ratio     = std::fabs (v_.rate);

    // Top line: channel, state, the one number that matters.
    const juce::String state = v_.frozen ? "LATCH" : (pitchPath ? (v_.reversed ? "REV" : "PITCH") : "DELAY");
    text (title_ + " " + state, top, juce::Justification::centredLeft, v_.frozen ? theme::oled : theme::oledDim, 10.0f);

    juce::String reading;
    if (pitchPath)
    {
        const float st = 12.0f * std::log2 (std::max (ratio, 1e-3f));
        reading = "x" + juce::String (ratio, 2) + " " + (st >= 0.0f ? "+" : "") + juce::String (st, 1) + "ST";
    }
    else
    {
        reading = ms (v_.delayMs);
    }
    text (reading, top, juce::Justification::centredRight, theme::oled, 11.0f);

    // Level: a column of small blocks, the display's own meter.
    {
        auto meter = inner.removeFromRight (8.0f * u).reduced (0.0f, 4.0f * u);
        const int n = 10;
        const float cell = meter.getHeight() / n;
        const float lvl = juce::jlimit (0.0f, 1.0f, v_.level);
        for (int i = 0; i < n; ++i)
        {
            const bool lit = lvl * n > static_cast<float> (i);
            g.setColour (lit ? (i >= n - 1 ? theme::ledRed : theme::oled) : theme::oledFaint);
            g.fillRect (meter.getX(), meter.getBottom() - cell * static_cast<float> (i + 1) + 1.0f,
                        meter.getWidth(), cell - 2.0f);
        }
    }

    // Ring.
    const auto  c = inner.getCentre();
    const float R = std::min (inner.getWidth(), inner.getHeight()) * 0.47f;

    auto arc = [&] (float a0, float a1, float radius, float width, juce::Colour col)
    {
        if (a1 < a0) a1 += kTwoPi;
        juce::Path p;
        p.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    };

    // The memory: a thin circle, with a dot every eighth.
    arc (0.0f, kTwoPi - 0.0001f, R, 1.0f * u, theme::oledDim);
    for (int i = 0; i < 8; ++i)
    {
        const auto p = c.getPointOnCircumference (R, static_cast<float> (i) / 8.0f * kTwoPi);
        g.setColour (theme::oledDim);
        g.fillRect (p.x - 1.0f * u, p.y - 1.0f * u, 2.0f * u, 2.0f * u);
    }

    if (pitchPath)
        arc (angleOf (v_.hi), angleOf (v_.lo), R, 5.0f * u, theme::oled.withAlpha (v_.frozen ? 0.9f : 0.55f));
    else
        arc (angleOf (v_.primary), v_.writePos * kTwoPi, R - 6.0f * u, 1.0f * u, theme::oledDim);

    // Each splice ticks at the entry bound.
    if (pitchPath && spliceFlash_ > 0.05f)
    {
        const float entry = angleOf (v_.reversed ? v_.lo : v_.hi);
        g.setColour (theme::oled.withAlpha (spliceFlash_));
        g.drawLine ({ c.getPointOnCircumference (R + 5.0f * u, entry),
                      c.getPointOnCircumference (R + 11.0f * u, entry) }, 1.5f * u);
    }

    // Write head: a radial bar through the ring, still when latched.
    {
        const float a = v_.writePos * kTwoPi;
        g.setColour (v_.frozen ? theme::oledDim : theme::oled);
        g.drawLine ({ c.getPointOnCircumference (R - 9.0f * u, a),
                      c.getPointOnCircumference (R + 9.0f * u, a) }, 2.0f * u);
    }

    // Read heads: square pixels.
    auto head = [&] (float delay, float gain)
    {
        const auto p = c.getPointOnCircumference (R, angleOf (delay));
        const float s = 7.0f * u;
        g.setColour (theme::oledBg);
        g.fillRect (p.x - s * 0.5f - 1.5f * u, p.y - s * 0.5f - 1.5f * u, s + 3.0f * u, s + 3.0f * u);
        g.setColour (theme::oled.withAlpha (juce::jlimit (0.2f, 1.0f, gain)));
        g.fillRect (p.x - s * 0.5f, p.y - s * 0.5f, s, s);
    };
    head (v_.primary, pitchPath ? v_.gainA : 1.0f);
    if (pitchPath && v_.gainB > 0.01f)
        head (v_.secondary, v_.gainB);

    // Centre: the memory layout.
    text (juce::String (v_.words) + (v_.quasi ? " SHARED" : " WORDS"),
          juce::Rectangle<float> (R * 1.4f, 12.0f * u).withCentre (c), juce::Justification::centred,
          theme::oledDim, 8.5f);

    // Bottom line: the control under your hand, otherwise the region.
    if (touchedTicks_ > 0)
    {
        text (touchedText_, bottom, juce::Justification::centredLeft, theme::oled, 10.0f);
    }
    else if (pitchPath)
    {
        text ("RGN " + ms (v_.lo * v_.msPerWord) + "-" + ms (v_.hi * v_.msPerWord), bottom,
              juce::Justification::centredLeft, theme::oledDim, 9.5f);
        if (v_.match > 0.0f)
            text ("JOIN " + juce::String (v_.match, 2), bottom, juce::Justification::centredRight, theme::oledDim, 9.5f);
    }
    else
    {
        text ("FIXED DELAY", bottom, juce::Justification::centredLeft, theme::oledDim, 9.5f);
    }

    // Scanlines, faint, so the glass reads as a display rather than panel.
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (float y = glass.getY(); y < glass.getBottom(); y += 2.0f * u)
        g.drawHorizontalLine (juce::roundToInt (y), glass.getX(), glass.getRight());
}
