# THE89TH

Private Mac VST3 effect plugin. A dual-channel pitch-shifting delay, cloned from a 1978 French studio box.

**Status:** the original's signal path is cloned as far as the published record allows. On top of it sit modern controls (link, feedback routing and tone, snap, sync, scrub), all off by default, plus presets with A/B compare. Custom interface. VST3 + Standalone, Apple Silicon. Version `0.0.1`.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

| Output | Path |
|--------|------|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/THE89TH.vst3` (installed after every build) |
| Standalone | `build/plugin/.../Standalone/THE89TH.app` |
| Renderer | `build/cli/the89th-render` |
| Interface snapshot | `build/plugin/the89th-snapshot` |

Needs CMake, Ninja and Apple clang. JUCE and Catch2 download on first configure.

**Rebuilding while FL is open:** FL keeps the build it loaded first until you quit it. Rescanning, "Reload plugin" and re-adding the plugin all return the old code, because macOS keeps a loaded plugin in memory. Quit and reopen FL after a rebuild, then check the build number in the top-right corner of the panel. For fast iteration without a host, `cmake --build build --target run_the89th` rebuilds and restarts the Standalone app.

## Validate

[pluginval](https://github.com/Tracktion/pluginval) loads the plugin the way a host does and stress-tests it: cold and warm opens, every sample rate and block size, state save and restore, parameter fuzzing, automation from other threads, and Steinberg's VST3 validator. Run it after changes to the processor or parameters:

```bash
scripts/validate.sh
```

That runs level 10, the strictest, five times with different random seeds, against the installed VST3 (pass another path to test a different build). It finds pluginval on the PATH or in `/Applications`; install it with `brew install --cask pluginval` or from its releases page. The tests use random values, so a single pass proves less than it looks. A failure prints its seed: `scripts/validate.sh <path> <seed>` repeats exactly that run.

It passes on every seed tried so far.

## How it works

Think of a tape loop with one recording head and two playback heads. The recording head runs at a fixed speed. The playback heads run at whatever speed the Pitch control sets, and playing back faster or slower than you record shifts the pitch. Two crosspoints fence off a stretch of memory. When a playback head reaches the far one it jumps back to the near one, and the two heads crossfade over the jump so you don't hear the join.

The signal path, per channel:

```
in → treble lift (delay mode only) → anti-alias filter → down to the machine's clock
   → converter (13-bit gain-ranging, clips at full scale) → memory
   → two read heads → crossfade → back to host rate → reconstruction filter
   → treble cut (delay mode only) → mix → out
                    ↑                        │
                    └──── feedback ──────────┘   (repeats go back through the pitch shifter)
```

The machine runs at its own clock, 52,910 Hz divided by the bandwidth setting. Memory is 16,384 words, split 8,192 per side in true stereo. Switching bandwidth changes only the clock, so whatever is stored replays at the new speed and jumps by an octave. That's the original's behaviour, not a bug.

## Using it

The panel opens at 1100 × 800 and resizes from 880 to 1760 wide, keeping its proportions. Orange means live: values, lit rings, active switches.

**PRESETS** (the strip along the top)
- **◀ ▶** step through presets; click the name for the full list. An asterisk means you've changed something since loading it.
- **Save** stores the panel as one of your presets, in `~/Library/Application Support/THE89TH/Presets`, one file each. The list also has *Delete* and *Show preset folder*. Factory presets can't be overwritten.
- **A / B** holds two complete settings. Switching keeps the one you leave, so you can flip back and forth while you tweak; the first switch copies the current setting across. **A>B** copies the side you're on to the other. Both slots are saved with the FL project.

**SYSTEM**
- **Mode.** *Delay* gives each channel a plain delay set by its Delay knob, with the treble lift. *Pitch* makes the heads move through the crosspoint region at the Pitch ratio.
- **Stereo.** *True* gives two independent channels, each with half the memory. *Quasi* feeds one input into the whole memory, so both channels play the same recording with their own pitch and crosspoints, and 20 kHz becomes available.
- **Range.** *Short* divides the Delay range by ten, for doubling and flanging.
- **Bandwidth.** 5, 10 or 20 kHz. Lower gives longer delays and grainier sound. 20 kHz needs Quasi.

**LATCH / OUTPUT**
- **Freeze** stops recording and keeps looping the crosspoint region, with pitch and reverse still active. It lights solid orange while latched.
- **Mix** goes from dry to wet.
- **Init** returns every control to its default and clears the memory. Hosts can't automate it, so a stray automation lane can't wipe a song's settings.

The next row is modern additions, not on the original. Each one does nothing at its default, so a fresh instance is still the 1978 machine.

**FEEDBACK LOOP**
- **Routing** (true stereo). *Norm*: each side feeds itself, as on the original. *Cross*: each side's repeats come back on the other side, ping-pong. *Sum*: both feed both.
- **Low cut / High cut / Drive** act inside the loop, so they build up with every repeat: a high cut darkens each pass a little more, drive thickens the repeats and rounds off loud ones. *Off* / 0 % at their defaults.

**MUSICAL**
- **Snap** pulls the Pitch knobs onto a scale, counted from the unshifted note: chromatic, major, minor or pentatonic.
- **Sync** makes Delay and both crosspoints step through note values (1/64 up to a whole note, with triplets and dotted) at FL's tempo. A note longer than the memory holds reads *> MAX* and plays at the longest length that fits.

**SCRUB** slides the crosspoint region back and forth, by up to its own length each way. *LFO* sways smoothly; *RND* wanders to a new spot each cycle. It needs a region smaller than the whole memory to have room to move.

**Each channel**
- **Delay:** the delay time in Delay mode. It fades when Pitch mode is on.
- **Pitch:** 0.25× to 2× (−24 to +12 semitones). Its ring lights from the centre, where the pitch is unchanged.
- **Fine:** ±100 cents on top of Pitch. Modern.
- **Crosspoint 1 / 2:** the region the heads play, shown in ms (or note values under Sync). Set Crosspoint 1 deeper than Crosspoint 2 and the region plays in **reverse**.
- **Feedback:** repeats go back through the pitch shifter, so they climb or fall in pitch.
- **Vibrato depth / speed / shape.** *SIN* is the original's. *SQR* is modern: in Pitch mode it jumps up by the depth and back, a trill; in Delay mode it alternates up and down, since a delay line can't stay sharp.
- **Link** (on channel 2's title line) makes channel 2 follow every channel 1 control. Channel 2's own knobs fade while it's on.

Double-click a knob to reset it. Controls the current mode ignores fade back rather than disappear.

Knob moves and automation don't click. Pitch, Feedback, Vibrato depth, Mix and the loop's cuts and drive glide to a new setting over 30 ms. Delay crossfades to its new time. A crosspoint pulled past a read head crossfades the head back into the region.

**The display** draws each channel's memory as a circle:
- a white tick sweeps round as the write head
- the orange arc is the crosspoint region
- the orange squares are the read heads, each as bright as it's loud, so you can watch a join crossfade
- the top line shows the mode and the pitch or delay
- the bottom line shows whichever control you're turning, and otherwise the region and how well the last join matched
- a level meter runs down the right side

## Renderer

WAV in, WAV out, no DAW needed:

```bash
build/cli/the89th-render in.wav out.wav --mode pitch --pitch 1.5 --xp1 0.1 --xp2 0.5 --feedback 0.6
```

Options: `--mode delay|pitch`, `--stereo true|quasi`, `--range long|short`, `--bandwidth 5|10|20`, `--delay`, `--pitch`, `--xp1`, `--xp2`, `--feedback`, `--mix`, `--vibrato`, `--vib-rate`, `--freeze`, `--freeze-after`, `--no-xing`. Modern: `--fine`, `--snap off|chromatic|major|minor|pentatonic`, `--vib-shape sine|square`, `--route normal|cross|sum`, `--lowcut`, `--highcut`, `--drive`, `--scrub`, `--scrub-rate`, `--scrub-mode lfo|random`. Every channel option sets both channels. Sync needs a host tempo, so it's plugin-only.

## Interface snapshot

```bash
build/plugin/the89th-snapshot out.png [width] [pitch|delay|freeze|quasi|modern]
```

Runs audio through the real plugin and renders the panel to a PNG, so you can check the interface without opening FL. The mockups the design came from are in [`docs/design/`](docs/design/).

## Layout

```
libs/core/     header-only DSP, no JUCE: converter, memory, read voices, Xing, filters, resampler
plugin/        VST3 + Standalone shell, the panel (src/gui), tests, snapshot tool
cli/           WAV → WAV renderer
scripts/       validate.sh: pluginval over several seeds
tests/         Catch2 / ctest
docs/          plain-English overview, clone status, design notes, hardware research, mockups
```

Start with [`docs/what-this-is.md`](docs/what-this-is.md) for the plain-English picture.

## How close it is

[`docs/clone-status.md`](docs/clone-status.md) goes through every behaviour and marks each one as matching, derived from published figures, assumed, or not modelled.

- **Matches or derived:** the two-head crosspoint mechanism, reverse, freeze, feedback through the pitch shifter, the clock-based bandwidth switch and its octave jumps, the 16,384 × 13-bit memory, true and quasi stereo, delay and pitch modes, the treble lift and cut, the short range, the converter and its clip, word-by-word reading, and the published band edges.
- **Assumed:** the filter type, the treble-lift curve, how the signal-aware join (Xing) picks its join points, the crossfade curve, and the vibrato ranges. Each is a single named constant, easy to change.
- **Not modelled:** the rear-panel control sockets (pitch clock, voltage control, insert loop) and the KB 2000 keyboard.

Nothing has been compared against a working unit yet. Recordings from one would settle most of the assumptions.

## Next

- Keyboard pitch control (the KB 2000 layer): MIDI notes set the pitch, note-off latches and mutes. Parked for now.
- Recordings from a working unit, to settle the assumed parts of the clone.

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
