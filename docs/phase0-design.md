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

At ratio 1.0 forward, `delayStep == 0`, no bound is ever reached, no splice fires, secondary stays silent. Transparency is structural rather than a tolerance.

Fixed length in Phase 0. `Xing` (autocorrelation splice-point search + adaptive segment length) later replaces *where* the splice fires and *how long* it lasts, behind the same interface.

### 2.1 Three corrections the tests forced

The sketch above was wrong in three places. Each was caught by a failing test, and each is worth recording because the naive version looks right on paper.

**Head placement follows the sign of the step, not crosspoint order.** Placing the head on crosspoint 2 because the research calls it "the start" puts a ratio-below-1.0 head on the bound it is about to leave: those drift *deeper* and exit at the far bound. It then spliced on sample one, into memory nothing had been written to yet. The entry bound is `step > 0 ? lo : hi`, full stop.

**The incoming head launches on the entry bound, not one region length behind the outgoing one.** A full-region offset is the textbook answer and it fails whenever the region spans most of memory: launching 96 samples early asks for a *negative* delay, `clampDelay` pins the head at the minimum, and a pinned head tracks the write pointer at 1× instead of the pitch ratio. Every fade then mixed in untransposed signal. At ratio 0.5 that measured as a 0.107 per-sample step where a clean 220 Hz tone can only reach 0.047 — the leak was the input's own 440 Hz slew, showing up exactly where the "no discontinuity" test looks. The fix offsets by the fade's own travel, `wrapped(primary) + activeFade * step`, which keeps both heads inside the region for the whole crossfade.

**The fade is capped at a quarter of the traversal, not a half.** Handover leaves the head one fade's travel inside the entry bound. At a half-region cap that lands it back in trigger range on the very next sample, and on a short region the handover position ratchets outward a little further on every splice until it walks out of memory entirely. A quarter leaves three fades of clearance.

Consequence worth knowing: a traversal is now the region shortened by one fade's travel, so the splice period is `regionLength/|step| - crossfadeSamples`, not `regionLength/|step|`.

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

28 test cases, 136k assertions, wired into ctest. Built and passing.

| File | What it pins down |
|---|---|
| `test_transparency` | ratio 1.0 forward: output `==` input delayed by exactly 8189 samples, bit-exact, under both interpolators. Plus: silence before the delay fills, and no splice ever fires |
| `test_pitch` | 440 Hz in, measured f0 out within 1% for ratios 2.0, 1.5, 1.0, 0.75, 0.5; reverse holds pitch at ratio 1.0 and composes with transposition; freeze keeps looping after the input stops |
| `test_splice` | six ratio/direction combinations: `max\|y[n]-y[n-1]\|` stays under twice the steepest step that output frequency can naturally take. Includes a negative control at fade length 1 that must *fail* that bound, so the threshold is proving something |
| `test_traversal` | pure state machine: step arithmetic for all six cases in §1.3, head placement on the entry bound, both heads inside memory across a splice, equal-power gains, splice period, degenerate region |
| `test_bandwidth` | rate table, the 8192-word split, 20 kHz clamping to 10 kHz in true stereo, published delay maxima, memory and pointers bit-identical across a switch, stored audio replaying an octave down at half the clock, and a live switch mid-stream |

The transparency test needs an interpolating kernel, meaning one that returns the exact sample at `frac == 0`. Linear and Catmull-Rom both do.

A note on measuring pitch in tests: a pure sine correlates just as well at every multiple of its period, so a plain autocorrelation argmax reports an arbitrary octave. `estimatePeriod` takes the shortest lag that is a local peak within 90% of the best. Getting this wrong reads a correct 880 Hz output as 440 Hz and looks exactly like a broken pitch shifter.

---

## 6. Seams, explicitly not implemented

| Later feature | Seam |
|---|---|
| Flying-comma quantiser (13-bit) | `Quantiser.hpp`, currently identity, called on write and read in `DelayMemory` |
| Anti-alias / reconstruction filters | `BandLimit.hpp`, currently identity, sits either side of the resampler. **Measurable today**: a 440 Hz tone rendered at 5 kHz bandwidth comes back with an image at 12787 Hz at 0.73 of the fundamental, and at 10 kHz one at 21985 Hz at 0.13. These are reconstruction images from an unfiltered upsample, not the hardware's ringing. Worth fixing before any serious A/B |
| Xing adaptive splice | `SpliceTraversal` decides *when* to fire and *how long* to fade; both become virtual policy |
| Vibrato / LFO / random position | additive offset applied to `delaySamples` before the read |
| MIDI, KB 2000 | control layer writing `ChannelParams`, no DSP change |
| GUI | `GenericAudioProcessorEditor` now, real editor later, processor untouched |
| Quasi-stereo routing | `Spec::channels = 1` already unlocks 16,384 words and the 20 kHz position. Only the input routing is missing. |
| Pre / de-emphasis | `BandLimit.hpp`, engaged in delay mode and bypassed in pitch mode, per the dossier |

---

## 7. Toolchain

Installed and building:

- cmake 4.4.2 and ninja 1.13.2, via Homebrew.
- Apple clang 17, arm64, Command Line Tools only. No full Xcode, so the **Ninja generator**. VST3 needs nothing more.
- JUCE 8.0.15 and Catch2 v3.15.3 fetched by CPM at configure time, pinned by tag and shallow-cloned. `dr_wav` vendored as a single public-domain header.
- **VST3 only.** AU dropped per your call, so `auval` and the Xcode question are both moot.

Build and test:

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The VST3 lands in `build/plugin/the89th_plugin_artefacts/RelWithDebInfo/VST3/` and copies itself to `~/Library/Audio/Plug-Ins/VST3/`. Verified loadable: `dlopen` on the bundle binary, `bundleEntry`, then `GetPluginFactory` reports vendor THE89TH and two classes (Audio Module + Component Controller).

Zero warnings under `juce_recommended_warning_flags`, which is stricter than the core's own build and caught 21 signed-index conversions in `Engine.hpp`.

JUCE 8 is GPLv3 or commercial. A private internal tool you never distribute is fine under GPLv3, which is also the basis for setting `JUCE_DISPLAY_SPLASH_SCREEN=0`. Flagging it so it is a decision rather than a discovery.

---

## 8. Naming compliance

No `Publison`, `DHM`, `DHM 89`, or `Infernal Machine` in any path, identifier, string, or metadata.

The dossier is now `docs/hardware-research.md`. Its **contents** still name the hardware throughout, which is correct: it is research about a real machine, not product metadata. The rule binds code, filenames, UI strings and plugin metadata.

One thing outside the repo: the enclosing folder is still `PUBLISON DHM 89 B2 CLONE`. It sits above the repo root, so nothing I generate references it, but you may want to rename it.

Plugin metadata as built:

| Field | Value |
|---|---|
| Plugin name | `THE89TH` |
| Company | `THE89TH` |
| Plugin code | `T89t` |
| Manufacturer code | `Lj89` |
| Bundle id | `com.the89th.the89th` |

Grep confirms no trademarked string in any source file, filename, or build output.

---

## 9. Parameters as shipped

One set drives both channels. The hardware has independent per-channel controls and `EngineParams` already carries separate `left` and `right` structs, so splitting them is a layout change in `PluginProcessor` and no DSP change at all.

| Parameter | Range | Default | Note |
|---|---|---|---|
| Pitch | 0.25 – 2.0, skewed to centre on 1.0 | 1.0 | displays as ratio and semitones |
| Crosspoint 1 | 0 – 1 | 0.0 | set above crosspoint 2 to reverse |
| Crosspoint 2 | 0 – 1 | 1.0 | |
| Feedback | 0 – 0.99 | 0.0 | pitch shifter is inside the loop |
| Mix | 0 – 1 | 1.0 | |
| Bandwidth | 5 / 10 / 20 kHz | 10 kHz | 20 kHz labelled "mono only" and clamps to 10 in stereo |
| Freeze | bool | off | |

No output limiting anywhere. Feedback at 0.7 with an equal-power splice measured a peak of 2.35 on a full-scale input. The hardware would have clipped into its converter; nothing here does, so it will run hot into whatever follows.
