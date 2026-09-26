# Clone status

What the plugin does, measured against what's known about the 1978 machine.

Every behaviour is in one of four states:

- **Matches**: follows documented behaviour, and a test checks it.
- **Derived**: not documented directly, but worked out from published numbers, and the numbers check out.
- **Assumed**: the hardware detail isn't published. We made a reasoned choice, and it's defined in one place in the code so it's easy to change.
- **Not modelled**: left out on purpose.

Modern controls sit on top of the clone (link, feedback routing and tone, snap, fine, sync, scrub, square vibrato, presets). None of them is the hardware. Each is bypassed at its default, so a fresh instance is still exactly what this page describes. They're covered in [`phase1-design.md`](phase1-design.md), not here.

Nothing here has been compared against a recording made for the purpose. Demo videos of real units have been checked (next section), which settled the panel layout and the keyboard; measuring the sound needs clean recordings.

## Sources checked on video

Ten YouTube videos of real units were checked in September 2026: transcripts, panel stills, and the stretches worth reading at 1080p. Timestamps are where the evidence is.

| Source | What it shows |
|---|---|
| Hainbach, "Ahead Of Its Time: Publison DHM89B2 (1978)", with Robert Henke (Manuel Göttsching's unit) | **2:24–2:29** Publison's two-page brochure for the DHM 89 B2 and KB 2000, readable. **2:30–2:46, 9:32–9:38** the KB 2000 panel. **9:22–9:25** its rear panel. **3:02–3:16, 5:02–5:10** a later unit's front panel with vibrato and the short/long switch. Talk: the bandwidth switch as instant octave switching; the tuning drifting because it comes from an analog oscillator; the keyboard fading notes in |
| scalenyc, "Publison DHM 89 B2 Stereo Digital Audio Computer demo" | An early unit without vibrato, lit clearly throughout: per-side Delay / Pitch-Shifter / Memory Latch buttons, the per-side display selector, displays in seconds and ratio |
| Robert Henke, PitchLoop89 masterclass part 1 (Synthtopia) | The first nine minutes walk through his unit: independent latch per channel, the reverse light, the self-similarity search at the joins, coarse and fine pitch pots |
| foleytronics, three "for Soundgas" videos and "A Deep Analysis" | Demos. The manual's advice that crosspoint 2 sit at least 10 ms from crosspoint 1; one octave up, not two (his correction). "A Deep Analysis", despite its title, is an instrumental demo with a Prophet-5 and no narration (transcribed locally to check) |
| MS-20 + DHM 89 B2; INTI FARI with the KB 2000 | Performances; nothing measurable |

What they changed here: Mode and Memory Latch became per side, and the KB 2000 was rebuilt from its panel and brochure. What they confirmed: the 52.91 kHz clock, the 300/600/1200 ms limits, −2 to +1 octaves, the 5/10/20 kHz switch with 20 kHz in quasi-stereo only, and "continuous variation of delay without doppler effect nor switching noises".

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
| Delay mode and Pitch mode, chosen per side; a Delay control per channel | Matches | Every photographed panel has Delay and Pitch-Shifter buttons on each side (scalenyc, Hainbach). The rear connector's "pitch mode" sets both at once | `ChannelParams::mode`, `ReadVoice` |
| Memory latch per side; in quasi-stereo either side's holds both | Matches | A Memory Latch button and light per side on every panel; Henke: the two channels latch independently. Quasi-stereo has one write | `ChannelParams::freeze`, `Machine::apply` |
| Pitch range 0.25× to 2× | Matches | Brochure: "from −2 to +1 octave" | `Musical`, plugin |
| Delay limits 1200 / 600 / 300 ms at 5 / 10 / 20 kHz | Matches | Brochure | `Spec` |
| A Delay change moves without pitch bend or clicks | Matches | Brochure: "continuous variation of delay without doppler effect nor switching noises". We crossfade to the new delay | `ReadVoice` |
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
| Pitch, feedback, vibrato depth and mix moves | Glide to the new setting over 30 ms | The pitch pot tuned an analog oscillator, which can't jump either. A host only updates once per block, so without a glide a pitch sweep steps | `Engine::kGlideSeconds` |
| Crosspoint moved past a read head | The head crossfades back into the region, as at the end of a traversal | Snapping it to the new edge would click. How the hardware's counters reacted is unpublished | `SpliceTraversal::setRegion` |
| Vibrato | Sine; depth 0–2 semitones; speed 0.1–10 Hz; same peak deviation in both modes | Later units had depth and speed pots; shape and ranges unpublished | `ReadVoice`, plugin ranges |
| Quasi-stereo input | Mono sum of left and right | "One input" feeds both sides; which pin is unclear | `Machine::step` |
| Fine pitch span | ±100 cents | The panel has Coarse and Fine pots; Fine's span is unpublished | Plugin |

## The KB 2000

Its panel (Hainbach's video) names every section, and Publison's brochure says what each does. Neither gives times, ranges or scales, so those are ours. What the sources fix is marked as such; everything else in this table is a choice, made in one place.

| Behaviour | Source | What we chose | Where |
|---|---|---|---|
| A key replaces the pitch pots of the sides it plays; silence latches and mutes | Research: the external pitch clock disconnects the pots, and holding it high latches and mutes, used for note-off | As sourced. "Silent" means the side's envelope has finished | `Keyboard::apply` |
| Left / Right / Biphonic | Panel switch; brochure: "three voice chorus", "biphonic memory synthesizer" | Biphonic: the two newest keys, lower on the left, higher on the right; one key plays both. Otherwise the newest key sounds | `Keyboard::allocate` |
| Push/Play and Sustain | Panel switch; brochure defines both | As defined | `Envelope` |
| Two envelopes: attack, hold, release | Panel; brochure: "two envelope generators drive two VCAs" | Straight-line segments. Attack 1 ms–2 s, hold 0–5 s (Sustain only), release 5 ms–5 s. Off: a 5 ms fade in and out. A new note restarts the attack from the current level | `Envelope`, plugin ranges |
| Glissando between notes | Panel "Slope"; brochure "glissando time" | 0–2 s, even in octaves. A new key on a sounding side glides without restarting; one that starts a note restarts the traversal | `Engine::retarget` |
| Trimmer | Panel; brochure: fine tune of the whole keyboard | ±100 cents | `Keyboard::apply` |
| Added delay | Panel; brochure: "a serial delay can be added, continuously adjustable" | Slides the crosspoint region deeper, by up to the room left behind it | `Keyboard::apply` |
| Root key, pitch bend | Not on the panel | C3 at unity (the three-octave span covers −2 to +1 octaves); bend ±2 st | `KeyboardParams::root`, `keys::bendSemitones` |
| Vibrato: frequency, sharpness, depth, each moved by a modulator the note's attack starts | Panel; brochure: "dual evolving vibrato" | Frequency 0.1–12 Hz; sharpness drives a sine into a soft clip towards square; depth 0–2 st. Modulator pull ±1 of: ±2 octaves, full sharpness, ±2 st. The modulator rises over its attack while the note is on and falls over its release after | `Keyboard::vibratoSemitones` |
| Memory Synchro: attack, end and return points, speed | Panel; brochure: each note starts at the attack point, loops between return and end while it sustains, speed sets the reading speed | Points run 0 (oldest) to 1 (newest) through memory; the crosspoint region keeps its length and slides to the reading position. Speed 0–2×; the panel's "Free" is taken as reading at the pitch, like tape | `Keyboard::process`, `Keyboard::apply` |
| Reverse Synchro | Panel ("for use on direct sound only", "automatic control of crosspoints by attacks of sounds"); brochure: keeps reversed sound in tempo, added delay adjustable | A peak follower (50 ms fall) with hysteresis at half the threshold; attacks at least 50 ms apart; each restarts the traversal after the delay (0–1 s). Ignored while latched | `Keyboard::process` |
| Noise gate | Panel switch in the Reverse Synchro section | Mutes the side's output while its input stays under half the threshold, 5 ms fades | `Keyboard::process` |
| ON / REM / OFF switches | Panel | REM (remote, over the rear connector) is left out: only on and off | Plugin |

## Not modelled

| Feature | Why |
|---|---|
| External TTL pitch clock (26–212 kHz), CV inputs for delay and crosspoints, ISS, insertion loop | These are ways of controlling the machine or patching into it, not part of its sound. A MIDI layer could stand in later |
| The KB 2000's microphone input and preamp, its mic/keyboard balance, and its cartridge-monitor output | Audio routing around the machine, which a DAW does |
| The display selector (Delay / Pitch ratio / Crosspoint 1 / Crosspoint 2 per side) | The plugin's display shows all of them at once |
| Drift of the pitch oscillator | Hainbach: the tuning comes from an analog oscillator and never sits still. The size and speed of the drift are unknown |
| D/A zero-order-hold droop | The published response is +0/−3 dB overall, so whatever droop existed is folded into the filters |
| Unit-to-unit differences (6-knob vs 8-knob panels, RAM variants, expanded 5 s memory) | We clone one configuration: a later unit, standard memory |

## Known limits of the plugin itself

- **20 kHz at a 44.1 kHz host.** The host's own Nyquist limit is 22.05 kHz, so the filters are capped at 0.45 × host rate. Run the host at 48 kHz or higher for the full 20 kHz band.
- **Fixed conversion latency.** The resampler needs more time at a slower clock. The plugin always reports the slowest clock's figure, about 3.3 ms, and pads the faster clocks to match. A changing latency would make the host restart the plugin, which empties the memory. The dry signal is held back by the same amount so it stays in time with the rest of the project.
- **Peaks can go above 0 dBFS.** Two things add up. Mid-crossfade, both heads play at once: on an equal-power fade the pair can sum to +3 dB where they happen to line up, and Xing brings that down by switching to equal gain when the join matches. And heavy feedback clips the converter, and the steep output filter overshoots on the clipped edges. Measured: up to about +3 dB from the machine, +5 dB after the filter, on a chord at 0.95 feedback. The analog stages of the original would do the same. A DAW works in floating point, so nothing clips inside the plugin; keep an eye on the channel meter at extreme feedback.
- **A running FL keeps the build it loaded first.** Quit and reopen FL after a rebuild; the build stamp tells you which build is loaded. See the README.

---

## Open questions

- **Word-by-word reading and the distortion figure.** The brochure quotes 0.1 % distortion in delay mode and 0.2 % in pitch mode. Measured on a 1 kHz sine at −1 dBFS and 10 kHz bandwidth: harmonic distortion 0.007–0.017 % in every mode, well inside. But at ratios whose read pattern repeats over a few samples, reading whole words puts image tones inside the band. At ×0.75 they sit at a quarter of the clock ± the output (5864 and 7364 Hz for a 750 Hz tone), at −28 dB, about 4 %; at ×1.5 they fall above the band and the filter removes them. Either Publison measured at a convenient ratio, or the output converter ran on each head's own read clock, which would give clean varispeed. Nothing public says which, so `Truncate` stays until a recording or the service manual settles it.
- **Delay in pitch mode.** The brochure's pitch-shifting paragraph says "the serial delay is adjustable"; Henke says the Delay pot does nothing in pitch mode. The KB 2000's Added Delay is modelled; the Delay pot in pitch mode is not.
- **Clock figures.** Henke quotes 40, 20 and 10 kHz sample rates. The brochure's 52.91 kHz is what's modelled.

## What would move "assumed" to "matches"

In order of how much each would settle:

1. **Recordings from a working unit.** Impulses through each bandwidth, sweeps, and a tone through pitch shifts at fixed settings. With those we could fit the filters, the crossfade and Xing to the real machine.
2. **The service manual's block diagram and IC map** (Studio Electronics, per the research doc). They'd confirm or correct the counter-based reading and the converter structure.
3. **The community IC/schematic set** mentioned in the research. It could identify the crossfade stage and the converter's parts.
