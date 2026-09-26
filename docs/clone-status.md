# Clone status

What the plugin does, measured against what's known about the 1978 machine.

Every behaviour is in one of four states:

- **Matches**: follows documented behaviour, and a test checks it.
- **Derived**: not documented directly, but worked out from published numbers, and the numbers check out.
- **Assumed**: the hardware detail isn't published. We made a reasoned choice, and it's defined in one place in the code so it's easy to change.
- **Not modelled**: left out on purpose.

Modern controls sit on top of the clone (link, feedback routing and tone, snap, fine, sync, scrub, square vibrato, presets). None of them is the hardware. Each is bypassed at its default, so a fresh instance is still exactly what this page describes. They're covered in [`phase1-design.md`](phase1-design.md), not here.

Nothing here has been compared against a working unit. That comparison is the one step that would turn "assumed" into "matches".

---

## Signal path

| Behaviour | State | Basis | Where |
|---|---|---|---|
| Fixed write head, two variable-rate read heads, crosspoint region, crossfade at the jump | Matches | Research doc; the machine's defining design | `SpliceTraversal`, `ReadVoice` |
| Reverse by setting crosspoint 1 deeper than crosspoint 2 | Matches | Research doc | `SpliceTraversal` |
| Freeze loops the region; pitch and reverse still work on it | Matches | Research doc | `SpliceTraversal`, `ReadVoice` |
| Feedback passes through the pitch shifter | Matches | Research doc | `Machine` |
| Bandwidth switch changes the clock, so switching jumps pitch by octaves | Matches | Research doc | `Machine`, `Engine` |
| RAM is 16,384 words of 13 bits | Derived | 13 DRAMs at 16384×1 bit. Reproduces 300/600/1200 ms to within rounding; reading the words as 16 bits misses by 17% | `Spec` |
| True stereo: 8,192 words per side, 20 kHz unreachable | Derived | Every true-stereo figure in the research matches 8,192 words. The shared converter is what caps the clock | `Spec`, `Machine` |
| Quasi-stereo: one input, one 16,384-word memory, 20 kHz, merged feedback | Matches | Research doc | `Machine` |
| Delay mode and Pitch mode; a Delay control per channel | Matches | Research doc; rear-connector "pitch mode"; the agolvet port | `ReadVoice` |
| Pre/de-emphasis in delay mode only | Matches | Research doc. Measured: >4 dB less converter noise in the treble, flat signal | `Emphasis` |
| Short/long range divides the delay scale by ten | Matches | Research doc | `ReadVoice` |
| Converter clips at full scale | Matches | Research doc | `FlyingComma` |
| Flying-comma word is sign + 9-bit mantissa + 3-bit exponent | Derived | The only 13-bit split that reaches the published ~95 dB (96.3 dB) and a 16-bit equivalent | `FlyingComma` |
| Reads take whole words, with no interpolation | Derived | The read address comes from a TTL counter clocked at the pitch rate. Nothing in a design without a multiplier can interpolate | `Truncate` |
| Band edge at 5/10/20 kHz, +0/−3 dB | Matches | Research doc. Measured within ±1 dB | `BandLimitFilter` |

## Parts built to published behaviour, details assumed

| Behaviour | What we chose | Why | Where to change it |
|---|---|---|---|
| Filter type | 8th-order Chebyshev I, 0.5 dB ripple, one each side of the converter, each −1.5 dB at the edge | Steep, rings at the edge like the low settings are described. Topology unpublished | `BandLimitFilter::kOrder`, `kRippleDb`, `kEdgeDb` |
| Emphasis curve | 50/15 µs | The standard for PCM gear of that era. Curve unpublished | `Emphasis::kT1`, `kT2` |
| Xing (signal-aware splice) | At each splice, search inward from the entry for the best waveform match, refined to a fraction of a sample | Research says the original "detects optimal transition points and adapts segment length". Circuit unpublished; Henke's version doesn't match either | `Xing` |
| Crossfade curve | Follows the match: equal gain when the joined material matches, equal power when it doesn't | The analog crossfade stage is unidentified | `SpliceTraversal::fadeGain` |
| Crossfade length | 96 samples at the internal clock | Unpublished | `Spec::crossfadeSamples` |
| Delay control change | Crossfade to the new delay | A jump would click and a glide would bend pitch; two playback circuits handing over is what the machine does elsewhere | `ReadVoice` |
| Pitch, feedback, vibrato depth and mix moves | Glide to the new setting over 30 ms | The pitch pot tuned an analog oscillator, which can't jump either. A host only updates once per block, so without a glide a pitch sweep steps | `Engine::kGlideSeconds` |
| Crosspoint moved past a read head | The head crossfades back into the region, as at the end of a traversal | Snapping it to the new edge would click. How the hardware's counters reacted is unpublished | `SpliceTraversal::setRegion` |
| Vibrato | Sine; depth 0–2 semitones; speed 0.1–10 Hz; same peak deviation in both modes | Later units had depth and speed pots; shape and ranges unpublished | `ReadVoice`, plugin ranges |
| Quasi-stereo input | Mono sum of left and right | "One input" feeds both sides; which pin is unclear | `Machine::step` |
| Freeze | One latch for both channels | The rear connector's latch acts on both | Plugin |

## Not modelled

| Feature | Why |
|---|---|
| External TTL pitch clock (26–212 kHz), CV inputs for delay and crosspoints, ISS, insertion loop | These are ways of controlling the machine or patching into it, not part of its sound. A MIDI layer could stand in later |
| Pitch input held high mutes and latches | Same: it's a control-interface behaviour |
| KB 2000 keyboard companion | Separate device |
| D/A zero-order-hold droop | The published response is +0/−3 dB overall, so whatever droop existed is folded into the filters |
| Unit-to-unit differences (6-knob vs 8-knob panels, RAM variants, expanded 5 s memory) | We clone one configuration: a later unit, standard memory |

## Known limits of the plugin itself

- **20 kHz at a 44.1 kHz host.** The host's own Nyquist limit is 22.05 kHz, so the filters are capped at 0.45 × host rate. Run the host at 48 kHz or higher for the full 20 kHz band.
- **Fixed conversion latency.** The resampler needs more time at a slower clock. The plugin always reports the slowest clock's figure, about 3.3 ms, and pads the faster clocks to match. A changing latency would make the host restart the plugin, which empties the memory. The dry signal is held back by the same amount so it stays in time with the rest of the project.
- **Peaks can go above 0 dBFS.** Two things add up. Mid-crossfade, both heads play at once: on an equal-power fade the pair can sum to +3 dB where they happen to line up, and Xing brings that down by switching to equal gain when the join matches. And heavy feedback clips the converter, and the steep output filter overshoots on the clipped edges. Measured: up to about +3 dB from the machine, +5 dB after the filter, on a chord at 0.95 feedback. The analog stages of the original would do the same. A DAW works in floating point, so nothing clips inside the plugin; keep an eye on the channel meter at extreme feedback.
- **A running FL keeps the build it loaded first.** Quit and reopen FL after a rebuild; the build stamp tells you which build is loaded. See the README.

---

## What would move "assumed" to "matches"

In order of how much each would settle:

1. **Recordings from a working unit.** Impulses through each bandwidth, sweeps, and a tone through pitch shifts at fixed settings. With those we could fit the filters, the crossfade and Xing to the real machine.
2. **The service manual's block diagram and IC map** (Studio Electronics, per the research doc). They'd confirm or correct the counter-based reading and the converter structure.
3. **The community IC/schematic set** mentioned in the research. It could identify the crossfade stage and the converter's parts.
