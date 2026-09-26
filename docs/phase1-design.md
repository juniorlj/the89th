# THE89TH: Phase 1 design proposal

Status: **for review. Paused while the clone was finished, and several items are now done as part of the hardware.**

> **Already done by the clone work** (see [`clone-status.md`](clone-status.md)):
> - Band-limit filters and converter clip (step 3), in their authentic form.
> - Per-side controls for pitch, crosspoints, feedback and delay (part of step 4). The hardware had them.
> - Vibrato (part of step 7): per channel, sine.
> - Millisecond readouts for delay and crosspoints (part of step 2).
> - Smoothing (part of step 2): pitch, feedback, vibrato depth and mix glide over 30 ms, and a crosspoint moved past a read head splices it back in rather than clicking. Crosspoints don't glide: they bound the region rather than being heard.
> - The custom GUI (step 9), now flat black and orange after mockup `design/mockup-02b-digital.png`, with a live display per channel. Built for the hardware's control set; modern controls still need a place in it.
>
> **Still to do from this plan:** fine and snap pitch, tempo sync, a link switch, cross and sum routing in true stereo (quasi-stereo already merges), feedback tone and drive, scrub, and presets with A/B.
>
> **Parameter IDs** below predate the clone work. The plugin now uses `pitch`/`pitch_r` as a ratio (the hardware's control), plus `delay`, `mode`, `stereo`, `range` and the vibrato IDs. Revise section 3 before building.

Scope: everything you picked.

- Modernize: custom GUI with a live memory view, real units and smooth controls, sound cleanup, presets with A/B.
- Functions: stereo play, musical pitch with tempo sync, feedback tone and drive, motion.

---

## 0. Ground rule: the 1:1 machine stays underneath

Every modern control has a **neutral position**, and at neutral it does nothing at all. The defaults are all neutral, so a fresh instance *is* the vintage machine. Init restores that.

| Modern control | Neutral |
|---|---|
| Fine | 0 cents |
| Pitch snap | Off |
| Sync | Off |
| Link | On (right follows left, like Phase 0) |
| Feedback routing | Normal (each side feeds itself) |
| Low cut / High cut | 20 Hz / 20 kHz |
| Drive | 0 % |
| Vibrato depth, Scrub depth | 0 |

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

**IDs are stable wherever the meaning is stable.** Pitch changes meaning (ratio → semitones), so it gets new IDs. An old 0.0.1 project will open at unity pitch, not a wrong pitch.

### Per side (L keeps the Phase 0 IDs)

| Control | L / R ID | Range | Default |
|---|---|---|---|
| Pitch | `semis` / `semis_r` | −24 … +12 st (the hardware's 0.25× … 2×) | 0 |
| Fine | `cents` / `cents_r` | −100 … +100 ct | 0 |
| Crosspoint 1 | `xp1` / `xp1_r` | ms, or note value when synced | shortest |
| Crosspoint 2 | `xp2` / `xp2_r` | ms, or note value when synced | longest |
| Freeze | `freeze` / `freeze_r` | on / off | off |

### Global

| Control | ID | Range | Default |
|---|---|---|---|
| Link | `link` | on / off | on |
| Bandwidth | `bandwidth` | 5 / 10 / 20 kHz (mono only) | 10 kHz |
| Feedback | `feedback` | 0 … 99 % | 0 % |
| Routing | `fb_route` | Normal / Cross / Sum | Normal |
| Low cut | `fb_lowcut` | 20 … 2000 Hz | 20 Hz |
| High cut | `fb_highcut` | 1 … 20 kHz | 20 kHz |
| Drive | `drive` | 0 … 100 % | 0 % |
| Pitch snap | `snap` | Off / Chromatic / Major / Minor / Pentatonic | Off |
| Sync | `sync` | on / off | off |
| Vibrato depth / rate / shape | `vib_depth` `vib_rate` `vib_shape` | 0 … 12 st · 0.1 … 10 Hz · Sine / Square-up | 0 · 5 Hz · Sine |
| Scrub depth / rate / mode | `scrub_depth` `scrub_rate` `scrub_mode` | 0 … 100 % of region · 0.05 … 10 Hz · LFO / Random | 0 · 0.5 Hz · LFO |
| Mix | `mix` | 0 … 100 % | 100 % |
| Init, Build | `init` `build` | as now | |

---

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
