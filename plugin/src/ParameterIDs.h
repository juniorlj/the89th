#pragma once

/** Single source of truth for parameter IDs. Changing one breaks saved state,
    so they live apart from the layout that builds them. The left channel keeps
    the Phase 0 IDs, which is why they carry no suffix. */
namespace pid
{
inline constexpr const char* build     = "build";

// Global: the hardware's switches. Mode and the latch are per channel, below.
inline constexpr const char* stereo    = "stereo";
inline constexpr const char* range     = "range";
inline constexpr const char* bandwidth = "bandwidth";
inline constexpr const char* mix       = "mix";
inline constexpr const char* init      = "init";

// Global: modern controls. Neutral by default; see docs/phase1-design.md.
inline constexpr const char* link       = "link";
inline constexpr const char* fbRoute    = "fb_route";
inline constexpr const char* lowCut     = "fb_lowcut";
inline constexpr const char* highCut    = "fb_highcut";
inline constexpr const char* drive      = "drive";
inline constexpr const char* snap       = "snap";
inline constexpr const char* sync       = "sync";
inline constexpr const char* scrubDepth = "scrub_depth";
inline constexpr const char* scrubRate  = "scrub_rate";
inline constexpr const char* scrubMode  = "scrub_mode";

// Keyboard layer (the KB 2000's pitch control). Off by default.
inline constexpr const char* keys       = "keys";
inline constexpr const char* keysRoot   = "keys_root";

/** Per-channel controls. Index 0 is left, 1 is right. */
struct Channel
{
    const char* mode;
    const char* freeze;
    const char* delay;
    const char* pitch;
    const char* crosspoint1;
    const char* crosspoint2;
    const char* feedback;
    const char* vibratoDepth;
    const char* vibratoRate;
    const char* fine;
    const char* vibratoShape;
};

inline constexpr Channel channel[2] = {
    { "mode",   "freeze",   "delay",   "pitch",   "xp1",   "xp2",   "feedback",   "vib_depth",   "vib_rate",   "fine",   "vib_shape"   },
    { "mode_r", "freeze_r", "delay_r", "pitch_r", "xp1_r", "xp2_r", "feedback_r", "vib_depth_r", "vib_rate_r", "fine_r", "vib_shape_r" },
};

// Phase 0 names for the left channel, kept for the tests and the editor.
// "mode" and "freeze" were global until each side got its own buttons; the
// left side keeps the IDs, and old state copies them to the right on load.
inline constexpr const char* mode        = channel[0].mode;
inline constexpr const char* freeze      = channel[0].freeze;
inline constexpr const char* pitch       = channel[0].pitch;
inline constexpr const char* crosspoint1 = channel[0].crosspoint1;
inline constexpr const char* crosspoint2 = channel[0].crosspoint2;
inline constexpr const char* feedback    = channel[0].feedback;
} // namespace pid
