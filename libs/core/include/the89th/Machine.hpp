#pragma once

#include <array>

#include "DelayMemory.hpp"
#include "Interpolation.hpp"
#include "Params.hpp"
#include "Quantiser.hpp"
#include "ReadVoice.hpp"
#include "Spec.hpp"

namespace the89th
{

/** The whole machine at its internal clock: RAM, converter, write head, two
    read voices, recirculation.

    True stereo: the RAM splits into two 8192-word memories, each with its own
    input, write and feedback. The converter is shared, so each side gets half
    the clock and 20 kHz is out of reach.

    Quasi-stereo: one input feeds both sides. A single write fills all 16384
    words at the full clock, both voices read that one memory with their own
    pitch and crosspoints, and 20 kHz becomes reachable. There is one write, so
    there is one recirculation: the two outputs merge before going back in, the
    single routing the research records for the hardware. The latch holds that
    one write, so it holds both sides.

    Switching layout repartitions the RAM, which empties it. The memories are
    allocated at full size up front, so switching never touches the heap. */
template <class Interp = Truncate, class Quant = FlyingComma>
class Machine
{
public:
    void prepare (const Spec& spec)
    {
        base_ = spec;
        for (auto& m : mem_)
            m.setSize (spec.memoryWordsTotal);
        repartition();
    }

    void reset()
    {
        for (auto& m : mem_)
            m.clear();
        state_ = { 0.0f, 0.0f };
        for (auto& v : voices_)
            v.reset();
    }

    void setParams (const EngineParams& p) noexcept
    {
        const bool relayout = p.stereo != params_.stereo;
        params_ = p;

        if (relayout)
            repartition();
        else
            apply();
    }

    void setXing (bool on) noexcept
    {
        for (auto& v : voices_)
            v.setXing (on);
    }

    void step (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float fbL = static_cast<float> (params_.left.feedback);
        const float fbR = static_cast<float> (params_.right.feedback);

        if (quasi())
        {
            const float in = 0.5f * (inL + inR);
            const float fb = 0.5f * (fbL * state_[0] + fbR * state_[1]);
            mem_[0].write (Quant::store (in + fb));

            outL = Quant::load (voices_[0].read (mem_[0]));
            outR = Quant::load (voices_[1].read (mem_[0]));
            voices_[0].advance (mem_[0]);
            voices_[1].advance (mem_[0]);
        }
        else
        {
            mem_[0].write (Quant::store (inL + fbL * state_[0]));
            mem_[1].write (Quant::store (inR + fbR * state_[1]));

            outL = Quant::load (voices_[0].read (mem_[0]));
            outR = Quant::load (voices_[1].read (mem_[1]));
            voices_[0].advance (mem_[0]);
            voices_[1].advance (mem_[1]);
        }

        state_ = { outL, outR };
    }

    double    internalSampleRate() const noexcept { return internalRate (spec_, params_.bandwidth); }
    Bandwidth effectiveBandwidth() const noexcept { return spec_.effectiveBandwidth (params_.bandwidth); }
    bool      quasi()              const noexcept { return params_.stereo == StereoMode::Quasi; }
    int       wordsPerVoice()      const noexcept { return spec_.memoryWordsPerChannel(); }

    const ReadVoice<Interp>& voice  (int i) const noexcept { return voices_[static_cast<std::size_t> (i)]; }
    const DelayMemory&       memory (int i) const noexcept { return mem_[static_cast<std::size_t> (i)]; }
    const EngineParams&      params()       const noexcept { return params_; }

private:
    void repartition() noexcept
    {
        spec_ = base_;
        spec_.channels = quasi() ? 1 : 2;

        const int words = spec_.memoryWordsPerChannel();
        for (auto& m : mem_)
            m.setWords (words);

        state_ = { 0.0f, 0.0f };
        for (auto& v : voices_)
            v.prepare (words, spec_.crossfadeSamples);

        apply();
        for (auto& v : voices_)
            v.reset();
    }

    void apply() noexcept
    {
        ChannelParams l = params_.left;
        ChannelParams r = params_.right;

        // One write in quasi-stereo, so one latch: holding it stops both sides.
        if (quasi())
            l.freeze = r.freeze = (l.freeze || r.freeze);

        mem_[0].setWriteHeld (l.freeze);
        mem_[1].setWriteHeld (r.freeze);

        const double fs = internalSampleRate();
        const ChannelParams* ps[2] = { &l, &r };
        for (std::size_t i = 0; i < voices_.size(); ++i)
        {
            voices_[i].setSampleRate (fs);
            voices_[i].setMode (params_.mode);
            voices_[i].setRange (params_.range);
            voices_[i].setParams (*ps[i]);
        }
    }

    Spec base_ {};
    Spec spec_ {};
    EngineParams params_ {};

    std::array<DelayMemory, 2>       mem_ {};
    std::array<ReadVoice<Interp>, 2> voices_ {};
    std::array<float, 2>             state_ { 0.0f, 0.0f };
};

using DefaultMachine = Machine<Truncate, FlyingComma>;

} // namespace the89th
