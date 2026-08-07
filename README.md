# THE89TH

Private Mac plugin. Dual-channel pitch-shifting delay, modelled on a 1978 French studio box: one write head, two moving read heads, crossfade at the splice.

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
