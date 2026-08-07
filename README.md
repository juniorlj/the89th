# THE89TH

Private Mac VST effect-plugin. Dual-channel pitch-shifting delay, modelled on a 1978 French studio box.

**Status:** Phase 0. VST3 + Standalone, Apple Silicon. Version `0.0.1`.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

| Output | Path |
|--------|------|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/THE89TH.vst3` (copied after build) |
| Standalone | `build/plugin/.../Standalone/THE89TH.app` |
| CLI | `build/cli/the89th-render` |

Needs CMake, Ninja, and Apple clang. JUCE and Catch2 download on first configure.

## Controls

Pitch · Crosspoint 1 · Crosspoint 2 · Feedback · Mix · Bandwidth · Freeze · Init

Crosspoint 1 deeper than Crosspoint 2 plays the region in reverse. Feedback runs through the pitch shifter. Bandwidth changes the converter clock (octave jumps on switch). 20 kHz is unavailable in true stereo.

## Layout

```
libs/core/     header-only DSP (no JUCE)
plugin/        VST3 + Standalone shell
cli/           WAV → WAV renderer
tests/         Catch2 / ctest
docs/          design notes and hardware research
```

Start with [`docs/what-this-is.md`](docs/what-this-is.md) for the plain-English picture.

## Not correct yet

The architecture matches the original (fixed write head, two reads, delay-domain reverse, clocked bandwidth). These parts are already in the path but do not behave like the hardware:

- **Splice** — fixed length, fixed timing. The original picks join points from the signal (Xing / autocorrelation) so the cut lands where the waveform already matches. Ours always jumps on schedule. Clean, less musical.
- **Low bandwidth** — no anti-alias or reconstruction filters. At 5 kHz a 440 Hz tone comes back with a loud image around 12.8 kHz. The original rings there; we alias.
- **Memory format** — full float into RAM. The original stores ≈13-bit flying-comma words (gain-ranged). Error should track level; ours is too clean.
- **Crossfade** — digital equal-power cosine/sine. The original used analog VCAs at the jump. Level handoff is right; the grain is not.
- **Host ↔ internal rate** — linear lerp resampler. The hardware A/D–D/A path is a different animal once the filters and converter land.
- **Feedback headroom** — no converter clip. Push feedback and we sail past full scale. The original hit its own converter and folded.
- **Stereo controls** — one knob set drives both channels. The hardware has independent pitch, crosspoints, and freeze per side (DSP already allows it; the UI does not).
- **Routing** — true stereo only. Quasi-stereo (shared input, full 16,384-word memory, 20 kHz) is not wired. At max clock the original also merged the two returns into feedback; we keep L/R separate.
- **Pitch input** — ratio knob only. The original also took a TTL pitch clock (≈26–212 kHz) and a rear CV/remote set.
- **Later-unit extras** — no vibrato depth/speed, no short/long memory range switch.

## Todo — full replication

- [ ] Flying-comma converter (≈13-bit gain-ranging store)
- [ ] Anti-alias and reconstruction filters (ringing, not aliasing)
- [ ] Xing — signal-informed splice points and adaptive fade length
- [ ] Pre / de-emphasis in delay mode (bypassed in pitch mode)
- [ ] Quasi-stereo routing + merged high-rate feedback path
- [ ] Per-channel UI (left/right already separate in the engine)
- [ ] Vibrato / LFO / random position scrub
- [ ] External pitch clock + MIDI / keyboard companion layer
- [ ] Real GUI (generic editor is temporary)
- [ ] Converter-style output clip on the feedback/write path

## Git

```bash
git status
git add -p
git commit -m "Why this change."
git push
```

`build/` is ignored. Do not commit render WAVs or IDE junk.

## Credits

Created by [juniorlj](https://github.com/juniorlj) and Cursor.
