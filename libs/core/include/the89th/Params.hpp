#pragma once

#include "Spec.hpp"

namespace the89th
{

struct ChannelParams
{
    /** Magnitude only, 0.25 to 2.0. Direction comes from crosspoint order, the
        same way the hardware's pitch clock carries frequency but no sign. */
    double pitchRatio = 1.0;

    /** Normalised 0..1 over the channel's memory. Crosspoint 2 is where the
        read head starts, crosspoint 1 is where it ends and splices back.
        Crosspoint 1 deeper than crosspoint 2 gives reverse. */
    double crosspoint1 = 0.0;
    double crosspoint2 = 1.0;

    /** The pitch shifter sits inside this loop, so repeats arpeggiate. */
    double feedback = 0.0;

    /** Memory latch: stop writing, keep looping the crosspoint region. */
    bool freeze = false;
};

struct EngineParams
{
    ChannelParams left;
    ChannelParams right;

    /** Global, because it is the converter clock rather than a per-channel filter. */
    Bandwidth bandwidth = Bandwidth::k20kHz;

    double mix = 1.0;  // 0 dry, 1 wet
};

} // namespace the89th
