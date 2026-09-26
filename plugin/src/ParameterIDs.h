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

// The original's keyboard controller, section by section as on its panel. All
// off by default; see KeyboardParams in the core for what each does.
inline constexpr const char* keys       = "keys";          // Off / Left / Right / Biphonic
inline constexpr const char* keysRoot   = "keys_root";
inline constexpr const char* kbPlay     = "kb_play";       // Push/Play or Sustain
inline constexpr const char* kbTrim     = "kb_trim";
inline constexpr const char* kbSlope    = "kb_slope";
inline constexpr const char* kbAdded    = "kb_added_delay";

inline constexpr const char* kbEnv      = "kb_env";
inline constexpr const char* kbAttack   = "kb_attack";
inline constexpr const char* kbHold     = "kb_hold";
inline constexpr const char* kbRelease  = "kb_release";

inline constexpr const char* kbVib        = "kb_vib";
inline constexpr const char* kbVibRate    = "kb_vib_rate";
inline constexpr const char* kbVibSharp   = "kb_vib_sharp";
inline constexpr const char* kbVibDepth   = "kb_vib_depth";
inline constexpr const char* kbVibModRate  = "kb_vib_mod_rate";
inline constexpr const char* kbVibModSharp = "kb_vib_mod_sharp";
inline constexpr const char* kbVibModDepth = "kb_vib_mod_depth";
inline constexpr const char* kbVibAttack  = "kb_vib_attack";
inline constexpr const char* kbVibRelease = "kb_vib_release";

inline constexpr const char* kbSynchro  = "kb_synchro";    // Off / Left / Right / Both
inline constexpr const char* kbAttackPt = "kb_attack_point";
inline constexpr const char* kbReturnPt = "kb_return_point";
inline constexpr const char* kbEndPt    = "kb_end_point";
inline constexpr const char* kbSpeed    = "kb_speed";

inline constexpr const char* kbReverse  = "kb_reverse";    // Off / Left / Right / Both
inline constexpr const char* kbGate     = "kb_gate";
inline constexpr const char* kbThresh   = "kb_threshold";
inline constexpr const char* kbRevDelay = "kb_rev_delay";

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
