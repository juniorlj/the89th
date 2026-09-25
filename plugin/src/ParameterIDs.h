#pragma once

/** Single source of truth for parameter IDs. Changing one breaks saved state,
    so they live apart from the layout that builds them. The left channel keeps
    the Phase 0 IDs, which is why they carry no suffix. */
namespace pid
{
inline constexpr const char* build     = "build";

// Global: the hardware's switches.
inline constexpr const char* mode      = "mode";
inline constexpr const char* stereo    = "stereo";
inline constexpr const char* range     = "range";
inline constexpr const char* bandwidth = "bandwidth";
inline constexpr const char* freeze    = "freeze";
inline constexpr const char* mix       = "mix";
inline constexpr const char* init      = "init";

/** Per-channel controls. Index 0 is left, 1 is right. */
struct Channel
{
    const char* delay;
    const char* pitch;
    const char* crosspoint1;
    const char* crosspoint2;
    const char* feedback;
    const char* vibratoDepth;
    const char* vibratoRate;
};

inline constexpr Channel channel[2] = {
    { "delay",   "pitch",   "xp1",   "xp2",   "feedback",   "vib_depth",   "vib_rate"   },
    { "delay_r", "pitch_r", "xp1_r", "xp2_r", "feedback_r", "vib_depth_r", "vib_rate_r" },
};

// Phase 0 names for the left channel, kept for the tests and the editor.
inline constexpr const char* pitch       = channel[0].pitch;
inline constexpr const char* crosspoint1 = channel[0].crosspoint1;
inline constexpr const char* crosspoint2 = channel[0].crosspoint2;
inline constexpr const char* feedback    = channel[0].feedback;
} // namespace pid
