# THE89TH

Private Mac VST effect-plugin. Dual-channel pitch-shifting delay, modelled on a 1978 French studio box.

**Status:** clone of the original's signal path complete, as far as the published record allows. VST3 + Standalone, Apple Silicon. Version `0.0.1`.

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

**Rebuilding while FL is open:** FL keeps the build it loaded first until you quit it. Rescanning, "Reload plugin" and re-adding the plugin all return the old code, because macOS keeps the plugin image resident in the process. Quit and reopen FL after a rebuild, and check the build stamp at the top of the editor. For fast DSP iteration without a host, `cmake --build build --target run_the89th` rebuilds and restarts the Standalone app.

## Controls

**Global:** Mode (Delay / Pitch) · Stereo (True / Quasi) · Range (Long / Short) · Bandwidth (5 / 10 / 20 kHz) · Freeze · Mix · Init

**Per channel, L and R:** Delay · Pitch · Crosspoint 1 · Crosspoint 2 · Feedback · Vibrato depth · Vibrato speed

- **Delay mode:** a fixed delay per channel, with pre/de-emphasis. **Pitch mode:** the heads move through the crosspoint region at the pitch ratio.
- Crosspoint 1 deeper than Crosspoint 2 plays the region in reverse.
- Feedback runs through the pitch shifter, so repeats climb or fall.
- Bandwidth changes the converter clock, so switching it jumps the pitch by octaves. 20 kHz needs Quasi-stereo.
- Freeze latches both channels and keeps looping the region.
- Delay and crosspoints read in ms, which change with bandwidth, stereo and range.

The window opens at 1100 × 680 and resizes from 880 to 1760 wide, keeping its proportions. Each channel has a live memory ring: the RAM drawn as a circle, with the write head sweeping round (amber), the read heads (teal) and the crosspoint region (purple). Splices flash as they happen. Double-click any knob to reset it.

To check the interface without a host: `build/plugin/the89th-snapshot out.png [width] [pitch|delay|freeze|quasi]` renders the editor to a PNG.

## Layout

```
libs/core/     header-only DSP (no JUCE)
plugin/        VST3 + Standalone shell
cli/           WAV → WAV renderer
tests/         Catch2 / ctest
docs/          design notes and hardware research
```

Start with [`docs/what-this-is.md`](docs/what-this-is.md) for the plain-English picture.

## How close it is

[`docs/clone-status.md`](docs/clone-status.md) goes through every behaviour and marks it as matching, derived from published numbers, assumed, or not modelled.

Short version:

- **Matches or derived:** the two-head crosspoint mechanism, reverse, freeze, feedback through the pitch shifter, the clock-based bandwidth switch and its octave jumps, the 16,384 × 13-bit RAM, true and quasi stereo, delay and pitch modes, emphasis, the short range, the flying-comma converter and its clip, word-by-word reading, and the +0/−3 dB band edges.
- **Assumed:** the filter type, the emphasis curve, how Xing picks its join points, the crossfade curve, and the vibrato ranges. Each is a named constant, so it's easy to change.
- **Not modelled:** the rear-panel control interface (TTL pitch clock, CV, insert loop) and the KB 2000.

Nothing has been compared against a working unit yet. Recordings from one would settle most of the assumptions.

## Next

Modern features, as designed in [`docs/phase1-design.md`](docs/phase1-design.md): presets, tempo sync, feedback tone, motion. Each one is neutral by default, so the clone underneath stays intact.

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
