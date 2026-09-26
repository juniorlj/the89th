# THE89TH: Phase 1 design proposal

Status: **built.** Everything in this plan is in, apart from two items dropped on purpose (see below). Each control is neutral by default, so a fresh instance is still the machine.

> **Done:**
> - Band-limit filters and converter clip (step 3), in their authentic form, as part of the clone.
> - Per-side pitch, crosspoints, feedback and delay (step 4). The hardware had them.
> - Units and smoothing (step 2): millisecond readouts; pitch, feedback, vibrato depth, mix and the loop tone glide over 30 ms; a crosspoint moved past a read head splices it back in rather than clicking.
> - Stereo play (step 4): Link, and Normal / Cross / Sum routing in true stereo.
> - Feedback tone and drive (step 5): low cut, high cut and drive inside the loop.
> - Musical pitch and tempo sync (step 6): Fine, Snap, and Sync for Delay and both crosspoints.
> - Motion (step 7): vibrato sine or square; Scrub by LFO or random.
> - Presets and A/B (step 8): 14 factory presets, user presets on disk, A/B saved with the project.
> - The custom GUI (step 9), flat black and orange, with the preset strip and a row for the modern controls.
>
> **Dropped:**
> - *Stereo lockstep (step 1)* was already how the clone runs: both sides step together at the internal clock.
> - *Per-side freeze*: the hardware's latch acts on both channels, and so does ours. (Superseded: panel photos show a Memory Latch and a Delay/Pitch-Shifter pair on each side, so both are per channel now. See `clone-status.md`.)
>
> **Section 3 is revised** for the clone's parameter set: every existing ID is kept, and the new controls are listed with their final IDs.

Scope: everything you picked.

- Modernize: custom GUI with a live memory view, real units and smooth controls, sound cleanup, presets with A/B.
- Functions: stereo play, musical pitch with tempo sync, feedback tone and drive, motion.

---

## 0. Ground rule: the 1:1 machine stays underneath

Every modern control has a **neutral position**, and at neutral it does nothing at all. The defaults are all neutral, so a fresh instance *is* the vintage machine. Init restores that.

| Modern control | Neutral |
|---|---|
| Fine | 0 cents |
| Snap | Off |
| Sync | Off |
| Link | Off (the two channels are independent, as on the hardware) |
| Feedback routing | Normal (each side feeds itself) |
| Low cut / High cut | 20 Hz / 20 kHz |
| Drive | 0 % |
| Scrub depth | 0 |
| Vibrato shape | Sine (the hardware's) |

I'm choosing neutral positions over a Vintage/Modern mode switch. With neutral positions you can add exactly one modern feature to the authentic machine. A mode switch would force all or nothing.

Two changes are **authentic corrections, not modern features**. They apply whether or not the modern controls are neutral:

- **Band-limiting filters** around the converter. The hardware had them; we don't yet. This removes the 12.8 kHz image at 5 kHz bandwidth.
- **Converter clip** on the write path. The hardware folded at its converter; ours runs past full scale.

Each gets a test that measures the correction.

---

## 1. Build order (one commit each, tests alongside)

| # | Step | Behaviour change |
|---|---|---|
| 1 | **Stereo lockstep.** Step both channels together at the internal rate instead of one after another. Needed for cross feedback | none: every existing test passes untouched |
| 2 | **Units + smoothing.** Crosspoints in ms (follows bandwidth), pitch in semitones + cents, % elsewhere. Pitch and crosspoints glide | display only, plus ~30 ms glide |
| 3 | **Sound cleanup.** 8th-order band-limit filters either side of the converter, cubic resampler, converter clip | authentic correction |
| 4 | **Stereo play.** Per-side pitch, fine, crosspoints, freeze; Link; routing Normal / Cross / Sum | neutral = Phase 0 |
| 5 | **Feedback tone + drive.** Low cut, high cut and saturation inside the loop, so they compound on every repeat | neutral = Phase 0 |
| 6 | **Musical pitch + tempo sync.** Scale snap; crosspoints lock to host tempo in note values | neutral = Phase 0 |
| 7 | **Motion.** Vibrato (sine, or square upward only); scrub moves the crosspoint region by LFO or random | neutral = Phase 0 |
| 8 | **Presets + A/B.** Factory set, user presets on disk, two compare slots | none |
| 9 | **Custom GUI + live memory view.** Built last, once the parameter set stops moving | none |

The GUI comes last on purpose. Building it first would mean rewiring it every time steps 4 to 7 add a control. Until then the generic editor keeps working, with the build stamp.

**Iteration loop.** FL can't pick up a rebuild without quitting (measured, see README). For steps 1 to 8, `cmake --build build --target run_the89th` rebuilds and relaunches the Standalone app in a few seconds. Check in FL at milestones.

---

## 2. One thing tempo sync can't hide: the memory is short

True stereo gives each side 8,192 words. That's **309.7 ms at 10 kHz** and **619.3 ms at 5 kHz**. At your 144 BPM:

| Note | Length | Fits at 10 kHz | Fits at 5 kHz |
|---|---|---|---|
| 1/16 | 104 ms | yes | yes |
| 1/8 | 208 ms | yes | yes |
| 1/8 dotted | 313 ms | **no** (3 ms over) | yes |
| 1/4 | 417 ms | no | yes |
| 1/4 dotted | 625 ms | no | **no** (6 ms over) |

Synced values that don't fit clamp to the longest length that does, and the display says so ("1/4 → max"). The short memory is the machine's character, so I'm not quietly expanding it to make sync friendlier.

---

## 3. Parameters

Revised after the clone work. **Every existing ID stays**, with its meaning, so saved projects open as they were. Pitch stays a ratio (the hardware's control); the musical controls act on top of it. Every new control has a neutral default, and at neutral it is bypassed outright, not just set to a small value.

### Kept from the clone (unchanged)

| Control | ID(s) |
|---|---|
| Mode, Stereo, Range, Bandwidth, Freeze, Mix, Init, Build | `mode` `stereo` `range` `bandwidth` `freeze` `mix` `init` `build` (Mode and Freeze later became per side: the left kept `mode` and `freeze`, the right got `mode_r` and `freeze_r`) |
| Per channel: Delay, Pitch (ratio), Crosspoint 1 and 2, Feedback, Vibrato depth and speed | `delay` `pitch` `xp1` `xp2` `feedback` `vib_depth` `vib_rate`, and each with `_r` for channel 2 |

### New, per channel

| Control | L / R ID | Range | Neutral |
|---|---|---|---|
| Fine | `fine` / `fine_r` | −100 … +100 cents, on top of Pitch | 0 |
| Vibrato shape | `vib_shape` / `vib_shape_r` | Sine / Square (jumps up by the depth and back, a trill) | Sine |

### New, global

| Control | ID | Range | Neutral |
|---|---|---|---|
| Link | `link` | Off / On. On: channel 2 follows every channel 1 control | Off |
| Feedback routing | `fb_route` | Normal / Cross / Sum. Cross: each side's repeats go into the other side, ping-pong. Sum: both into both. True stereo only; quasi-stereo already merges | Normal |
| Low cut | `fb_lowcut` | 20 … 2000 Hz, inside the feedback loop | 20 Hz (off) |
| High cut | `fb_highcut` | 1 … 20 kHz, inside the loop | 20 kHz (off) |
| Drive | `drive` | 0 … 100 %, saturation inside the loop | 0 % (off) |
| Snap | `snap` | Off / Chromatic / Major / Minor / Pentatonic. Pitch lands on the nearest step of the scale, counted from unison | Off |
| Sync | `sync` | Off / On. Delay and both crosspoints step through note values at the host tempo | Off |
| Scrub depth | `scrub_depth` | 0 … 100 %: how far the crosspoint region wanders, as a share of its own length | 0 % |
| Scrub speed | `scrub_rate` | 0.05 … 10 Hz | 0.5 Hz |
| Scrub mode | `scrub_mode` | LFO (smooth back and forth) / Random (wanders to a new spot each cycle) | LFO |

36 parameters in all, up from 22.

**What the loop does, in order.** Each side's output goes through low cut, high cut and drive, then through the routing, then into a memory, where the converter clips it as before. So the tone compounds on every repeat.

**Pitch.** Ratio from the Pitch knob → snapped to the scale if Snap is on → Fine added → clamped to the hardware's 0.25× … 2×. The core still sees one ratio.

**Sync note values**, shortest to longest: 1/64, 1/32T, 1/32, 1/16T, 1/32D, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1/2D, 1/1, with the knob's bottom position still the shortest delay. A value longer than the memory holds clamps to the longest that fits, and the readout says so (see section 2).

**Presets** store every parameter except Init and Build. A/B holds two complete settings; both are saved with the project.

## 4. Where each piece lives

The core stays header-only and free of JUCE. Anything that needs the host (tempo, presets, GUI) stays in the plugin.

```
libs/core/include/the89th/
  Smoother.hpp        NEW   glide that lands exactly on target, so transparency survives
  Lfo.hpp             NEW   sine, square-up, smoothed random
  FeedbackTone.hpp    NEW   low cut + high cut + drive + converter clip, run inside the loop
  BandLimit.hpp       REAL  8th-order Chebyshev I per band edge (5 / 10 / 20 kHz), anti-alias + reconstruction
  Resampler.hpp       CHG   stereo lockstep, cubic interpolation instead of linear
  ChannelEngine.hpp   CHG   feedback sample comes in from outside (routing), per-sample pitch and region modulation
  Engine.hpp          CHG   routing matrix, smoothing, LFOs, filters around the adapter
  Params.hpp          CHG   new fields, all defaulting to neutral

plugin/src/
  ParameterIDs.h      CHG
  PluginProcessor.*   CHG   semis + cents + snap → ratio; sync → crosspoints; link; publishes telemetry
  Presets.{h,cpp}     NEW   factory set, ~/Library/Application Support/THE89TH/Presets, A/B slots
  Telemetry.h         NEW   lock-free snapshot of head positions for the display
  gui/                NEW   editor, memory ring, knob look (step 9)
```

Pitch stays a plain ratio inside the core. Semitones, cents and snap are plugin-side conversions, so the core keeps one pitch concept.

---

## 5. Tests that will exist when this is done

- **Neutral is exact.** With every modern control at neutral, `ChannelEngine` output is bit-identical to Phase 0. All 31 current tests keep passing, and the transparency test stays bit-exact.
- **Filters.** 440 Hz at 5 kHz bandwidth: the 12.8 kHz image drops from −2.7 dB (today) to below −60 dB. Band edge within ±1 dB of −3 dB.
- **Converter clip.** Feedback 99 % with a full-scale input never exceeds full scale.
- **Smoothing.** A pitch or crosspoint jump produces no per-sample step larger than the audio itself makes.
- **Routing.** Left input only, Cross: right output carries the repeats. Sum: both do.
- **Tone.** High cut in the loop lowers the spectral centroid on every repeat.
- **Sync.** 144 BPM at 1/8 gives 208.33 ms within one sample. 1/4 dotted at 10 kHz clamps.
- **Snap.** 5.3 st → 5 chromatic; major-scale mapping across two octaves.
- **Motion.** Vibrato measured as f0 modulation at the set rate and depth. Scrub keeps both heads inside memory.
- **Presets.** Save and recall round trip exactly; A/B swap restores both slots.

---

## 6. GUI (step 9)

The centerpiece is **two memory rings**, one per side. Each ring is the 8,192-word memory drawn as a circle:

- a bright write head that sweeps round at the clock rate
- the crosspoint region as an arc
- the two read heads, one of them flashing through each splice as it crossfades
- frozen memory dimmed

Controls sit under each ring in groups: Pitch, Region, Feedback, Motion. The top bar holds presets, A/B, Init and the build stamp.

The look is original. Per the research doc, copying the hardware's panel artwork, layout or logo is the trade-dress risk, so nothing here imitates it.

A mockup accompanies this proposal. The final build will differ in detail.

---

## 7. Costs and risks

- **Parameter count** goes from 9 to 28. The GUI groups them. In FL's own parameter list they'll read as a long list.
- **CPU.** Eight filter stages per side at host rate, plus two LFOs and smoothing. Expect well under 1 % of one Apple Silicon core. I'll measure it rather than assume.
- **Saved 0.0.1 projects** open with pitch at unity. Everything else recalls.
- **The splice is still fixed-length.** Xing (signal-aware join points) isn't in this list. It's still the biggest thing between this and the original's sound.
