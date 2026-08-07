#pragma once

/** Single source of truth for parameter IDs. Changing one breaks saved state,
    so they live apart from the layout that builds them. */
namespace pid
{
inline constexpr const char* pitch       = "pitch";
inline constexpr const char* crosspoint1 = "xp1";
inline constexpr const char* crosspoint2 = "xp2";
inline constexpr const char* feedback    = "feedback";
inline constexpr const char* mix         = "mix";
inline constexpr const char* bandwidth   = "bandwidth";
inline constexpr const char* freeze      = "freeze";
} // namespace pid
