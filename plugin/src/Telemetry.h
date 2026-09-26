#pragma once

#include <array>
#include <atomic>

/** What the editor shows of the machine, published by the audio thread once per
    block and read by the editor's timer. Plain atomics: a slightly stale or
    mixed-block view is fine for drawing, and nothing here blocks audio. */
struct Telemetry
{
    struct Voice
    {
        std::atomic<float> primary   { 0.0f };   // delay of the carrying head, in words
        std::atomic<float> secondary { 0.0f };   // delay of the incoming head, in words
        std::atomic<float> gainA     { 1.0f };
        std::atomic<float> gainB     { 0.0f };
        std::atomic<float> regionLo  { 0.0f };
        std::atomic<float> regionHi  { 0.0f };
        std::atomic<float> match     { 0.0f };   // Xing correlation at the last splice
        std::atomic<float> peak      { 0.0f };   // output peak this block
        std::atomic<float> rate      { 1.0f };   // signed read rate: pitch ratio, negative in reverse
        std::atomic<float> delayMs   { 0.0f };   // delay mode: the Delay setting
        std::atomic<bool>  traversal { true };
        std::atomic<bool>  splicing  { false };
        std::atomic<bool>  reversed  { false };
    };

    std::array<Voice, 2> voice;

    /** Write position of each side's memory, 0..1 of its words. In quasi-stereo
        both sides read one memory, so both entries hold the same value. */
    std::array<std::atomic<float>, 2> writePos { 0.0f, 0.0f };

    std::atomic<int>   words     { 8192 };
    std::atomic<float> msPerWord { 1000.0f / 26455.0f };
    std::atomic<bool>  quasi     { false };
    std::atomic<bool>  frozen    { false };
    std::atomic<bool>  delayMode { false };
};
