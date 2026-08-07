# THE89TH — Phase 0 design proposal

Status: **for review, nothing implemented yet.**

Scope is exactly the six Phase 0 items. Everything else gets a named seam and no code.

## Decisions locked

| # | Decision |
|---|---|
| 1 | Reverse is a **signed read rate**, sign taken from crosspoint order. Ratio stays a positive magnitude. No reverse branch. |
| 2 | Memory is the **authentic 16,384 words** for the whole machine. No expansion option. |
| 3 | **Authentic true stereo**: 8,192 words per channel, and the 20 kHz bandwidth position is unreachable. |
| 4 | Dossier renamed to `docs/hardware-research.md`. Done. |

---

## 1. Two derivations that shape the architecture

### 1.1 Memory is 16,384 words per channel, and the word is ~13 bits

The dossier reports 210 kbit of RAM, 13× MB8116, and maxima of 300 / 600 / 1200 ms at 20 / 10 / 5 kHz.
The MB8116 is organised **16384 × 1 bit**, so 13 of them ganged in parallel give **16,384 words of 13 bits**, not 13,312 words of 16 bits. Checking both against the manual:

| Bandwidth | Internal rate | 16384 words (13-bit) | 13312 words (16-bit) | Manual |
|---|---|---|---|---|
| 20 kHz | 52910 Hz | 309.7 ms | 251.6 ms | 300 ms |
| 10 kHz | 26455 Hz | 619.3 ms | 503.2 ms | 600 ms |
| 5 kHz  | 13227.5 Hz | 1238.6 ms | 1006.4 ms | 1200 ms |

16,384 matches to within the manual's rounding. 13,312 is off by 17% at every setting. 16,384 is also 2^14, which is what an address counter and a 0–5 V crosspoint comparator want.

Two consequences:

- Total buffer length is a fixed **16,384 words**, independent of host sample rate. It is a memory size, not a time. True stereo splits it (§1.1b).
- The Phase 2 quantiser is **13-bit flying-comma**, not 16-bit. That is what "quasi-16-bit" in the dossier means: ~13 bits of storage buying 16-bit-class dynamic range through the exponent. Worth recording now even though the quantiser is out of scope.

### 1.1b True stereo: 8,192 words per channel, and 20 kHz is unreachable

You picked authentic true stereo, which splits the 16,384 words into 8,192 per channel. Running the numbers against the dossier's stated maxima:

| | 20 kHz | 10 kHz | 5 kHz |
|---|---|---|---|
| True stereo, 8192 words | 154.8 ms | **309.7 ms** | **619.3 ms** |
| Dossier, true stereo | *not listed* | ~300 ms | ~600 ms |
| Quasi-stereo, 16384 words | **309.7 ms** | **619.3 ms** | **1238.6 ms** |
| Dossier, quasi-stereo | 300 ms | 600 ms | 1200 ms |

Every listed figure matches. The gap is the 20 kHz true-stereo cell, which the dossier never gives, and that is the tell.

The dossier attributes 20 kHz being quasi-only to memory being halved, which does not actually follow: halved memory would just make 20 kHz *short* (155 ms), not impossible. **Converter throughput is the real constraint.** There is one converter running at 52,910 Hz. Quasi-stereo digitises one input at the full clock, which supports 20 kHz. True stereo time-shares that converter across two channels, so each gets 26,455 Hz, and 10 kHz is the ceiling. The missing table cell and the throughput argument agree, so I am treating the dossier's memory explanation as garbled and modelling the throughput.

In the engine: `Bandwidth` stays a 3-way parameter, because the hardware's front-panel switch is 3-way regardless of routing. In true stereo the engine clamps the request to 10 kHz and reports the difference through `effectiveBandwidth()`. When quasi-stereo lands in a later phase, the 20 kHz position becomes live with no parameter change.

### 1.2 The engine must run at the internal rate, or the octave jumps cannot happen

The bandwidth switch changes the converter clock (52910 / 26455 / 13227.5 Hz). Memory contents and pointer positions are addresses, so they survive the switch untouched. Material written at 52910 and then read at 26455 replays an octave down. That is the whole mechanism.

Running the engine at host rate and merely rescaling the max delay would reproduce the delay times and lose the jumps. So:

```
host rate  ──[down]──▶  internal rate (52910 / 26455 / 13227.5)  ──[up]──▶  host rate
                              │
                        ChannelEngine
                        16384-word buffer
```

`setBandwidth()` changes only the rate constant and the resampler ratios. It never touches the buffer, the write pointer, or the read-head delays. The jump falls out.

The resampler is a Phase 0 seam: a plain interface with a decent-quality default now, band-limited anti-alias and reconstruction filters later.

### 1.3 Reverse falls out of a signed read rate

Work in the **delay domain**: `delay` = samples behind the write head. With the write head advancing 1 sample per sample and the read head advancing `r`:

```
read_pos  = write_pos - delay
d(delay)/dt = 1 - r
```

`r` is signed. Its magnitude is the pitch ratio (from the pitch clock, always positive on the hardware). Its **sign comes from crosspoint order**, which is the mechanism the dossier describes for reverse:

```cpp
signedRate = (xp1Delay > xp2Delay ? -1.0 : +1.0) * pitchRatio;
delayStep  = 1.0 - signedRate;
```

The region is `[min(xp1,xp2), max(xp1,xp2)]`. The head drifts, hits a bound, wraps to the other by ±L. One code path, no reverse branch. Checks:

| xp order | ratio | delayStep | behaviour |
|---|---|---|---|
| xp1 < xp2 | 1.0 | 0 | delay frozen, no splice, **bit-transparent** |
| xp1 < xp2 | 2.0 | −1 | read advances 2×, octave up |
| xp1 < xp2 | 0.5 | +0.5 | read advances 0.5×, octave down |
| xp1 > xp2 | 1.0 | +2 | read walks backward at 1×, **reverse at pitch** |
| xp1 > xp2 | 2.0 | +3 | reverse, octave up |

The `agolvet/dhm89-web-audio` worklet uses the same delay-domain increment (`delay += (1 - ratio)/sampleRate`) and the same two-sided wrap, which corroborates the formulation. It has one read pointer, no crossfade, no signed rate, so reverse and the splice masking are ours to add.

---

## 2. Crossfade: active/standby, not a continuous 50/50 window

The textbook two-head shifter runs both heads permanently at half-region offset under a raised-cosine window. That **cannot be bit-transparent at ratio 1.0**: two taps always sum, and you get a comb filter.

The hardware crossfades *at the jump*, via VCAs. So:

- One **primary** head carries the signal at unity gain.
- When primary comes within `crossfadeSamples` of the far bound, the **secondary** launches at the near bound and an equal-power fade runs for exactly `crossfadeSamples`.
- At the end the roles swap. Between splices, secondary gain is exactly 0.

At ratio 1.0 forward, `delayStep == 0`, no bound is ever reached, no splice fires, secondary stays silent. Transparency is structural rather than a tolerance. During a fade the two heads sit a constant `L - crossfadeSamples` apart in delay.

Fixed length in Phase 0. `Xing` (autocorrelation splice-point search + adaptive segment length) later replaces *where* the splice fires and *how long* it lasts, behind the same interface.

---

## 3. File and module layout

```
THE89TH/
├── CMakeLists.txt                    top level, options, CPM bootstrap
├── cmake/
│   └── CPM.cmake                     pinned JUCE 8 + Catch2 v3 + dr_wav
├── libs/core/                        header-only, ZERO JUCE, zero allocation in process
│   ├── CMakeLists.txt                INTERFACE target the89th::core
│   └── include/the89th/
│       ├── Spec.hpp                  clock, memory words, bandwidth table
│       ├── Params.hpp                POD parameter structs
│       ├── Interpolation.hpp         Linear + Hermite policies
│       ├── DelayMemory.hpp           16384-word circular buffer + write pointer
│       ├── SpliceTraversal.hpp       signed rate, region wrap, two-head crossfade
│       ├── ChannelEngine.hpp         memory + traversal + feedback + freeze
│       ├── Resampler.hpp             host <-> internal rate  (SEAM)
│       ├── Quantiser.hpp             pass-through           (SEAM: flying-comma)
│       ├── BandLimit.hpp             pass-through           (SEAM: anti-alias)
│       └── Engine.hpp                two ChannelEngines + mix, the facade
├── plugin/
│   ├── CMakeLists.txt                juce_add_plugin, VST3 + AU
│   └── src/
│       ├── PluginProcessor.h/.cpp    APVTS -> core Params, GenericAudioProcessorEditor
│       └── ParameterIDs.h            single source of truth for IDs
├── cli/
│   ├── CMakeLists.txt                the89th-render, links core only, no JUCE
│   └── src/main.cpp                  WAV in -> WAV out via dr_wav
├── tests/
│   ├── CMakeLists.txt                Catch2 + catch_discover_tests -> ctest
│   ├── test_traversal.cpp            wrap, signed rate, region bounds
│   ├── test_transparency.cpp         ratio 1.0 bit-exact
│   ├── test_pitch.cpp                ratio 2.0 doubles measured f0
│   ├── test_splice.cpp               no discontinuity above threshold
│   └── test_bandwidth.cpp            switch preserves buffer, produces the jump
└── docs/
    ├── phase0-design.md              this file
    └── hardware-research.md          the dossier, renamed
```

Rationale on two choices you invited alternatives for:

- **Header-only core**: agreed, and mostly non-template. Only the interpolator is a policy parameter. Everything else is a plain class, so compile times stay sane and the debugger stays readable.
- **CLI without JUCE**: `dr_wav` is a single public-domain header. The CLI then links `the89th::core` alone and builds in a second, which is what makes A/B iteration fast. JUCE's `AudioFormatManager` would drag the whole framework into the render path.

---

## 4. Core engine public interface

```cpp
// ─── Spec.hpp ───────────────────────────────────────────────────────────────
namespace the89th {

enum class Bandwidth { k5kHz, k10kHz, k20kHz };

struct Spec {
    double converterClockHz = 52910.0;  // 18.9 us conversion time
    int    memoryWordsTotal = 16384;    // 2^14 for the whole machine
    int    channels         = 2;        // true stereo: memory and converter both split
    int    crossfadeSamples = 96;       // fixed splice fade (Phase 0)

    constexpr int memoryWordsPerChannel() const { return memoryWordsTotal / channels; }

    // One converter shared across channels. In true stereo each gets half the
    // clock, which is what puts 20 kHz out of reach.
    constexpr Bandwidth maxBandwidth() const {
        return channels > 1 ? Bandwidth::k10kHz : Bandwidth::k20kHz;
    }
    constexpr Bandwidth effectiveBandwidth(Bandwidth requested) const {
        return requested > maxBandwidth() ? maxBandwidth() : requested;
    }
};

constexpr int    rateDivisor (Bandwidth);            // 20k->1, 10k->2, 5k->4
constexpr double internalRate(const Spec&, Bandwidth);
constexpr double maxDelaySeconds(const Spec&, Bandwidth);


// ─── Params.hpp ─────────────────────────────────────────────────────────────
struct ChannelParams {
    double pitchRatio  = 1.0;   // 0.25 .. 2.0, magnitude only; sign comes from crosspoints
    double crosspoint1 = 0.0;   // 0..1 of memory. "end"   of the traversed region
    double crosspoint2 = 1.0;   // 0..1 of memory. "start" of the traversed region
    double feedback    = 0.0;   // 0..1, pitch shifter is inside this loop
    bool   freeze      = false; // stop writing, keep reading
};

struct EngineParams {
    ChannelParams left, right;
    Bandwidth     bandwidth = Bandwidth::k20kHz;  // global: it is the converter clock
    double        mix       = 1.0;                // 0 = dry, 1 = wet
};


// ─── SpliceTraversal.hpp ────────────────────────────────────────────────────
// Region traversal + two-head crossfade, in the delay domain. No audio, no buffer.
// Deterministic and fully inspectable, which is what makes the splice testable.
class SpliceTraversal {
public:
    struct Tap { double delaySamples; float gain; };

    void setRegion   (double xp1DelaySamples, double xp2DelaySamples);
    void setRatio    (double pitchRatio);        // magnitude
    void setCrossfade(int samples);
    void reset();                                // park primary at xp2, secondary muted

    void advance();                              // exactly one internal sample

    Tap  primary()   const noexcept;
    Tap  secondary() const noexcept;
    bool splicing()  const noexcept;
    double signedRate() const noexcept;          // for tests
};


// ─── DelayMemory.hpp ────────────────────────────────────────────────────────
class DelayMemory {
public:
    void  setSize(int words);                    // 8192 in true stereo
    void  clear();
    void  write(float x);                        // advances write pointer
    void  holdWrite(bool frozen);                // freeze: pointer and contents stand still
    template <class Interp> float readDelayed(double delaySamples) const;
    int   writePosition() const noexcept;
};


// ─── ChannelEngine.hpp ──────────────────────────────────────────────────────
class ChannelEngine {
public:
    void prepare(const Spec&);                   // allocates once, here
    void reset();

    // Rate only. Buffer contents, write pointer and head delays are deliberately
    // left untouched, which is what produces the octave jump.
    // Clamped by Spec::effectiveBandwidth: 20 kHz is unreachable in true stereo.
    void      setBandwidth(Bandwidth requested);
    Bandwidth effectiveBandwidth() const noexcept;

    void setParams(const ChannelParams&);        // crosspoints scaled by memoryWords

    float processSample(float in) noexcept;      // AT INTERNAL RATE
    void  process(const float* in, float* out, int n) noexcept;

    const SpliceTraversal& traversal() const noexcept;   // test access
    const DelayMemory&     memory()    const noexcept;
};


// ─── Engine.hpp ─────────────────────────────────────────────────────────────
// Facade: rate conversion + two channels + dry/wet. What the plugin and CLI use.
class Engine {
public:
    void prepare(double hostSampleRate, int maxBlockSize, const Spec& = {});
    void reset();
    void setParams(const EngineParams&);
    void process(float* const* io, int numChannels, int numSamples) noexcept;
    double latencySamples() const noexcept;

    ChannelEngine& channel(int i) noexcept;      // test + CLI access
};

} // namespace the89th
```

Signal flow inside `ChannelEngine::processSample`, which is where feedback topology is decided:

```
                 ┌─────────────────────────────────────────┐
                 │                                         │
   in ──▶(+)─────┴──▶ DelayMemory.write ──▶ two heads ──▶ crossfade ──┬──▶ out
          ▲                (held when frozen)                         │
          │                                                           │
          └────────────────── × feedback ◀────────────────────────────┘
```

The feedback tap is post-crossfade and pre-mix, so repeats re-enter the pitch shifter and arpeggiate. Freeze halts the write, which also stops feedback accumulating, matching the hardware.

---

## 5. Test plan (Phase 0 item 4)

All against `ChannelEngine` at its internal rate, bypassing the resampler. This matters for the transparency test: the host-to-internal resampler is a rate conversion, so a plugin-level round trip can never be bit-exact. Transparency is a property of the delay path, and that is what gets asserted.

| Test | Assertion |
|---|---|
| `transparency` | ratio 1.0, xp1 < xp2, integer delay: output == input delayed by exactly D samples, bit-exact (`==`, not `Approx`) |
| `pitch_ratio` | 440 Hz sine, ratio 2.0: measured f0 of output is 880 Hz ±1% (autocorrelation or parabolic-interpolated FFT peak) |
| `splice` | full-scale sine through many splices: `max |y[n] - y[n-1]|` never exceeds the per-sample delta the same sine reaches naturally, plus margin |
| `traversal` | pure state machine: wrap fires at the right bound, signed rate sign follows crosspoint order, delay stays in region |
| `bandwidth_jump` | write at 26455, switch to 13227.5: buffer contents and write pointer unchanged, measured f0 halves |
| `bandwidth_clamp` | in true stereo, requesting 20 kHz yields `effectiveBandwidth() == k10kHz` and an internal rate of 26455 Hz |

The transparency test needs an interpolating kernel, meaning one that returns the exact sample at `frac == 0`. Linear and Catmull-Rom both do.

---

## 6. Seams, explicitly not implemented

| Later feature | Seam |
|---|---|
| Flying-comma quantiser (13-bit) | `Quantiser.hpp`, currently identity, called on write and read in `DelayMemory` |
| Anti-alias / reconstruction filters | `BandLimit.hpp`, currently identity, sits either side of the resampler |
| Xing adaptive splice | `SpliceTraversal` decides *when* to fire and *how long* to fade; both become virtual policy |
| Vibrato / LFO / random position | additive offset applied to `delaySamples` before the read |
| MIDI, KB 2000 | control layer writing `ChannelParams`, no DSP change |
| GUI | `GenericAudioProcessorEditor` now, real editor later, processor untouched |
| Quasi-stereo routing | `Spec::channels = 1` already unlocks 16,384 words and the 20 kHz position. Only the input routing is missing. |
| Pre / de-emphasis | `BandLimit.hpp`, engaged in delay mode and bypassed in pitch mode, per the dossier |

---

## 7. Toolchain reality check

Verified on this machine:

- `cmake` is **not installed**. Blocker. `brew install cmake ninja`.
- Only Command Line Tools, no full Xcode. `xcodebuild` is unavailable, so builds use the **Ninja generator**. JUCE 8 dropped the Rez step for AUv2, so an AU bundle is expected to build under CLT. I will verify by running `auval` rather than assume it.
- `auval`, clang 17, git, Homebrew, arm64: all present.
- JUCE 8 and Catch2 v3 fetched by CPM at configure time, pinned by tag. Nothing to install by hand.

JUCE 8 is GPLv3 or commercial. A private internal tool that you never distribute is fine under GPLv3. Flagging it only so it is a decision rather than a discovery.

---

## 8. Naming compliance

No `Publison`, `DHM`, `DHM 89`, or `Infernal Machine` in any path, identifier, string, or metadata.

The dossier is now `docs/hardware-research.md`. Its **contents** still name the hardware throughout, which is correct: it is research about a real machine, not product metadata. The rule binds code, filenames, UI strings and plugin metadata.

One thing outside the repo: the enclosing folder is still `PUBLISON DHM 89 B2 CLONE`. It sits above the repo root, so nothing I generate references it, but you may want to rename it.

Plugin metadata I will use unless you say otherwise:

| Field | Value |
|---|---|
| Plugin name | `THE89TH` |
| Company | `THE89TH` |
| Plugin code | `T89t` |
| Manufacturer code | `Lj89` |
| Bundle id | `com.the89th.the89th` |

AU requires at least one uppercase character in the manufacturer code, which `Lj89` satisfies.
