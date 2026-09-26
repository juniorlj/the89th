#include "MemoryRing.h"
#include "Theme.h"

#include <cmath>

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

juce::String msText (float ms)
{
    return ms < 100.0f ? juce::String (ms, 1) + " ms" : juce::String (juce::roundToInt (ms)) + " ms";
}
} // namespace

MemoryRing::MemoryRing (const Telemetry& t, int channel, juce::String title)
    : t_ (t), channel_ (channel), title_ (std::move (title))
{
    setInterceptsMouseClicks (false, false);
}

void MemoryRing::refresh()
{
    const auto& tv = t_.voice[static_cast<std::size_t> (channel_)];

    v_.writePos  = t_.writePos[static_cast<std::size_t> (channel_)].load();
    v_.primary   = tv.primary.load();
    v_.secondary = tv.secondary.load();
    v_.gainA     = tv.gainA.load();
    v_.gainB     = tv.gainB.load();
    v_.lo        = tv.regionLo.load();
    v_.hi        = tv.regionHi.load();
    v_.match     = tv.match.load();
    v_.traversal = tv.traversal.load();
    v_.splicing  = tv.splicing.load();
    v_.reversed  = tv.reversed.load();
    v_.words     = std::max (1, t_.words.load());
    v_.msPerWord = t_.msPerWord.load();
    v_.frozen    = t_.frozen.load();
    v_.delayMode = t_.delayMode.load();
    v_.quasi     = t_.quasi.load();

    // Peak meter: fast attack, slow fall.
    const float peak = tv.peak.load();
    v_.level = peak > v_.level ? peak : v_.level * 0.86f;

    if (v_.splicing && ! wasSplicing_)
        spliceFlash_ = 1.0f;
    else
        spliceFlash_ *= 0.82f;
    wasSplicing_ = v_.splicing;

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
    auto bounds = getLocalBounds().toFloat();
    const float u = std::min (bounds.getWidth(), bounds.getHeight()) / 300.0f;  // design unit

    // Panel.
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 10.0f * u);

    // Header: channel name, memory layout, output level.
    auto header = bounds.reduced (14.0f * u, 10.0f * u).removeFromTop (20.0f * u);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f * u, true).withExtraKerningFactor (0.14f));
    g.drawText (title_, header, juce::Justification::centredLeft);

    const juce::String layout = juce::String (v_.words) + " words" + (v_.quasi ? "  shared" : "");
    g.setColour (theme::textFaint);
    g.setFont (theme::font (10.5f * u));
    g.drawText (layout, header.withTrimmedLeft (70.0f * u), juce::Justification::centredLeft);

    auto meter = header.removeFromRight (70.0f * u).withSizeKeepingCentre (70.0f * u, 5.0f * u);

    // Key to the ring's colours.
    {
        auto key = header.withTrimmedRight (18.0f * u).removeFromRight (230.0f * u);
        const std::pair<juce::Colour, const char*> items[] = {
            { theme::accent, "write" }, { theme::read, "read" }, { theme::region, "region" } };
        g.setFont (theme::font (10.5f * u));
        const float slotW = key.getWidth() / 3.0f;
        for (const auto& [col, name] : items)
        {
            auto slot = key.removeFromLeft (slotW);
            auto dot  = slot.removeFromLeft (10.0f * u).withSizeKeepingCentre (7.0f * u, 7.0f * u);
            g.setColour (col);
            g.fillEllipse (dot);
            g.setColour (theme::textFaint);
            g.drawText (name, slot.withTrimmedLeft (4.0f * u), juce::Justification::centredLeft);
        }
    }
    g.setColour (theme::line);
    g.fillRoundedRectangle (meter, 2.5f * u);
    const float lvl = juce::jlimit (0.0f, 1.0f, v_.level);
    g.setColour (lvl > 0.95f ? theme::danger : theme::read.withAlpha (0.85f));
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * lvl), 2.5f * u);

    // Ring geometry.
    auto ringArea = bounds.withTrimmedTop (34.0f * u).withTrimmedBottom (22.0f * u).reduced (10.0f * u);
    const auto c   = ringArea.getCentre();
    const float R  = std::min (ringArea.getWidth(), ringArea.getHeight()) * 0.40f;
    const float tw = R * 0.11f;   // track width

    auto arc = [&] (float a0, float a1, float radius, float width, juce::Colour col)
    {
        if (a1 < a0) a1 += kTwoPi;
        juce::Path p;
        p.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    // Memory track, with a faint tick every eighth of the RAM.
    arc (0.0f, kTwoPi - 0.0001f, R, tw, theme::panelHi.brighter (0.05f));
    g.setColour (theme::line);
    for (int i = 0; i < 8; ++i)
    {
        const float a = static_cast<float> (i) / 8.0f * kTwoPi;
        g.drawLine ({ c.getPointOnCircumference (R + tw * 0.5f + 2.0f * u, a),
                      c.getPointOnCircumference (R + tw * 0.5f + 6.0f * u, a) }, 1.0f);
    }

    const bool pitchPath = v_.traversal;
    const float dim = v_.frozen ? 0.55f : 1.0f;

    // Crosspoint region: from the deep bound (older, further behind the write
    // head) round to the shallow one.
    if (pitchPath)
        arc (angleOf (v_.hi), angleOf (v_.lo), R, tw, theme::region.withAlpha (0.55f));
    else
        arc (angleOf (v_.primary), v_.writePos * kTwoPi, R - tw * 0.9f, 2.0f * u, theme::accent.withAlpha (0.35f));

    // Splice flash at the entry bound.
    if (spliceFlash_ > 0.02f && pitchPath)
    {
        const float entry = angleOf (v_.reversed ? v_.lo : v_.hi);
        juce::Path p;
        p.addCentredArc (c.x, c.y, R + tw * 1.05f, R + tw * 1.05f, 0.0f, entry - 0.16f, entry + 0.16f, true);
        g.setColour (theme::read.withAlpha (0.8f * spliceFlash_));
        g.strokePath (p, juce::PathStrokeType (2.5f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Write head.
    {
        const float a = v_.writePos * kTwoPi;
        const auto col = v_.frozen ? theme::textFaint : theme::accent;
        g.setColour (col);
        g.drawLine ({ c.getPointOnCircumference (R - tw * 0.8f, a),
                      c.getPointOnCircumference (R + tw * 0.8f, a) }, 3.0f * u);
        juce::Path tri;
        const auto tip = c.getPointOnCircumference (R + tw * 0.9f, a);
        tri.addTriangle (tip,
                         c.getPointOnCircumference (R + tw * 1.9f, a - 0.05f),
                         c.getPointOnCircumference (R + tw * 1.9f, a + 0.05f));
        g.fillPath (tri);
    }

    // Read heads, as bright as they are loud.
    auto head = [&] (float delay, float gain)
    {
        const auto p = c.getPointOnCircumference (R, angleOf (delay));
        const float r = tw * 0.62f;
        g.setColour (theme::bg);
        g.fillEllipse (p.x - r - 1.5f * u, p.y - r - 1.5f * u, (r + 1.5f * u) * 2.0f, (r + 1.5f * u) * 2.0f);
        g.setColour (theme::read.withAlpha (juce::jlimit (0.15f, 1.0f, gain) * dim + (1.0f - dim) * 0.3f));
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
    };
    head (v_.primary, pitchPath ? v_.gainA : 1.0f);
    if (pitchPath && v_.gainB > 0.01f)
        head (v_.secondary, v_.gainB);

    // Centre readout.
    const auto tv = t_.voice[static_cast<std::size_t> (channel_)].rate.load();
    const float ratio = std::fabs (tv);
    auto centre = juce::Rectangle<float> (R * 1.3f, R * 1.1f).withCentre (c);

    juce::String big, small, state;
    if (pitchPath)
    {
        const float st = 12.0f * std::log2 (std::max (ratio, 1e-3f));
        big   = juce::String::fromUTF8 ("\xc3\x97") + juce::String (ratio, 2);
        small = (st >= 0.0f ? "+" : "") + juce::String (st, 2) + " st";
        state = v_.frozen ? "LATCHED" : (v_.reversed ? "REVERSE" : "PITCH");
    }
    else
    {
        big   = msText (t_.voice[static_cast<std::size_t> (channel_)].delayMs.load());
        small = "fixed delay";
        state = "DELAY";
    }

    g.setColour (v_.frozen ? theme::accent : theme::textDim);
    g.setFont (theme::font (10.5f * u, true).withExtraKerningFactor (0.16f));
    g.drawText (state, centre.removeFromTop (centre.getHeight() * 0.24f), juce::Justification::centredBottom);

    g.setColour (theme::text);
    g.setFont (theme::font (30.0f * u, true));
    g.drawText (big, centre.removeFromTop (centre.getHeight() * 0.48f), juce::Justification::centred);

    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f * u));
    g.drawText (small, centre.removeFromTop (centre.getHeight() * 0.45f), juce::Justification::centredTop);

    // Region span and last join match, below the ring.
    if (pitchPath)
    {
        auto foot = juce::Rectangle<float> (bounds.getWidth(), 16.0f * u)
                        .withCentre ({ c.x, bounds.getBottom() - 14.0f * u });
        const juce::String span = "region " + msText (v_.lo * v_.msPerWord) + juce::String::fromUTF8 (" \xe2\x80\x93 ") + msText (v_.hi * v_.msPerWord);
        const juce::String join = v_.match > 0.0f ? juce::String::fromUTF8 ("   \xc2\xb7   join match ") + juce::String (v_.match, 2) : juce::String();
        g.setColour (theme::textFaint);
        g.setFont (theme::font (10.5f * u));
        g.drawText (span + join, foot, juce::Justification::centred);
    }
}
