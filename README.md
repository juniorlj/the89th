# THE89TH

Private Mac VST3 effect plugin for FL Studio and Ableton Live. A dual-channel pitch-shifting delay, cloned from a 1978 French studio box.

Architected by juniorljj. Programmed by Cursor, Codex and Claude.

**Status:** the original's signal path is cloned as far as the published record allows, and so is its companion keyboard, played over MIDI. On top sit modern controls (link, feedback routing and tone, snap, sync, scrub), all off by default, plus presets with A/B compare. Custom interface. VST3 + Standalone, Apple Silicon; runs in FL Studio and Ableton Live. Version `0.5.0`. The user manual is [`docs/manual/`](docs/manual/index.html).

## Install on a Mac

THE89TH is built from source on your own Mac. It takes about ten minutes the first time, most of it downloading. The finished plugin installs itself where FL Studio and Ableton Live look for it.

**You need**
- A Mac with Apple Silicon (M1 or later) running macOS 11 Big Sur or newer. Intel Macs aren't supported.
- FL Studio or Ableton Live, or neither: a standalone app is built as well.
- Access to this GitHub repository, which is private.

**1. Install the build tools.** Open Terminal (Applications › Utilities) and run these one at a time. The first installs Apple's command-line tools; click *Install* in the window that appears and wait for it to finish.

```bash
xcode-select --install
```

The second installs [Homebrew](https://brew.sh), the Mac package manager. Skip it if `brew --version` already prints a version.

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Then CMake and Ninja, which drive the build:

```bash
brew install cmake ninja
```

**2. Download the code.**

```bash
git clone https://github.com/juniorlj/the89th.git ~/the89th
```

**3. Build and install.** The first build downloads JUCE and takes a few minutes; later builds are quick.

```bash
cd ~/the89th && cmake -S . -B build -G Ninja && cmake --build build
```

When it ends with `THE89TH: installed`, the plugin is at `~/Library/Audio/Plug-Ins/VST3/THE89TH.vst3`.

**4. Load it in your DAW.**
- **FL Studio:** *Options › Manage plugins*, click *Find plugins*, then load THE89TH in a mixer insert.
- **Ableton Live:** *Settings › Plug-Ins*, turn on *Use VST3 Plug-In System Folders*, click *Rescan*, then drag THE89TH from *Plug-Ins* in the browser onto an audio or return track.
- **No DAW:** open `build/plugin/the89th_plugin_artefacts/RelWithDebInfo/Standalone/THE89TH.app`.

The [user manual](docs/manual/index.html) covers every control, playing it from a MIDI keyboard in both DAWs, and starting-point recipes.

**Updating.** Pull the new code, rebuild, then quit and reopen your DAW. A running DAW keeps the old build in memory, even after a rescan. The Build plate at the top right of the plugin shows which version is loaded.

```bash
cd ~/the89th && git pull && cmake --build build
```

**Uninstalling.** Delete the plugin. Your saved presets live in `~/Library/Application Support/THE89TH/Presets`; delete that folder too if you want them gone.

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/THE89TH.vst3
```

**If it doesn't show up**
- Check the file exists: `ls ~/Library/Audio/Plug-Ins/VST3/`.
- In Live, check *Use VST3 Plug-In System Folders* is on and rescan.
- Quit and reopen the DAW; both keep their plugin list from startup.
- Copied the plugin from someone else's Mac instead of building it? macOS blocks downloaded plugins that aren't notarized. Clear the download flag, then rescan:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/THE89TH.vst3
```

## Build (for development)

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

| Output | Path |
|--------|------|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/THE89TH.vst3` (installed after every build) |
| Standalone | `build/plugin/the89th_plugin_artefacts/RelWithDebInfo/Standalone/THE89TH.app` |
| Renderer | `build/cli/the89th-render` |
| Interface snapshot | `build/plugin/the89th-snapshot` |

Builds are arm64 only, for macOS 11 and later, and ad-hoc signed. JUCE and Catch2 download on first configure. For fast iteration without a host, `cmake --build build --target run_the89th` rebuilds and restarts the Standalone app.

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
- **A / B** holds two complete settings. Switching keeps the one you leave, so you can flip back and forth while you tweak; the first switch copies the current setting across. **A>B** copies the side you're on to the other. Both slots are saved with the project.

**SYSTEM** holds the switches both channels share.
- **Stereo.** *True* gives two independent channels, each with half the memory. *Quasi* feeds one input into the whole memory, so both channels play the same recording with their own pitch and crosspoints, and 20 kHz becomes available.
- **Range.** *Short* divides the Delay range by ten, for doubling and flanging.
- **Bandwidth.** 5, 10 or 20 kHz. Lower gives longer delays and grainier sound. 20 kHz needs Quasi.

**OUTPUT**
- **Mix** goes from dry to wet.
- **Init** returns every control to its default and clears the memory. Hosts can't automate it, so a stray automation lane can't wipe a song's settings.

**KEYS** plays the machine from a MIDI keyboard, as the original's KB 2000 keyboard controller did. The KB 2000's layout and behaviour come from its own panel and Publison's brochure (see [`docs/clone-status.md`](docs/clone-status.md)); the times and ranges on it are ours, since neither gives scales.
- **Channels:** *Off* (the default), *L*, *R* or *BI*. *BI* is the KB 2000's biphonic mode: two voices, the lower key on the left, the higher on the right. One key plays on both.
- **Play:** *PUSH* (Push/Play) sounds only while a key is down. *SUST* (Sustain) starts the note on the key and lets the envelope decide how long it lasts.
- **Root:** the key that plays at the original pitch, C3 by default. Each key is a semitone, within the machine's −24 to +12. Pitch bend adds ±2 semitones.
- A key replaces the Pitch and Fine knobs of the sides it plays; they fade on the panel. The newest key sounds; lift it and the one held before comes back.
- When a side falls silent it **latches and mutes**, as the hardware did when its pitch input was held high. The memory keeps what was playing, so the next key replays it at a new pitch. Latch it yourself and the keyboard plays a frozen loop like a sampler.
- Notes land on the exact sample they're played. The display shows the key, or MUTE.
- **KEYBOARD** opens the keyboard's panel in place of the channels:
  - **Pitch ratio:** *Trimmer* tunes the whole keyboard (±100 cents). *Slope* glides from one note to the next (off to 2 s). *Added delay* pushes the region deeper, a delay in series with the shift.
  - **Envelope** (*On* in its title line): attack, hold and release on each side's output. Off, notes just fade in and out over 5 ms.
  - **Vibrato** (*On*): frequency, sharpness (sine towards square) and depth, and a modulator that each note starts: *Mod freq / sharp / depth* set how far it pulls each one, *Mod attack / release* how fast.
  - **Memory Synchro:** for latched memory. Each note starts reading at the *Attack pt*, runs to the *End pt*, then loops from the *Return pt* while the note lasts. Points count from the oldest sound in memory. *Speed* 1× reads as recorded, slower or faster stretches time without changing pitch; *Free* reads at the pitch, like tape. The row of lights shows where each side is reading.
  - **Reverse Synchro:** for live input. Each attack in the input restarts the side's traversal after *Delay*, so reversed segments keep the original's tempo. *Threshold* is what counts as an attack. *Gate* mutes the side while the input stays below it.
- **FL Studio:** effects get MIDI by port. Add a **MIDI Out** channel, give it a port number, and set the same number as this plugin's MIDI input port in its wrapper settings (gear icon). Notes on the MIDI Out channel, or your keyboard with it selected, then play the plugin.
- **Ableton Live:** make a MIDI track, set its *MIDI To* to the track holding THE89TH and pick THE89TH in the box below, then set the MIDI track's Monitor to *In* (or arm it).

The next row is modern additions, not on the original. Each one does nothing at its default, so a fresh instance is still the 1978 machine.

**FEEDBACK LOOP**
- **Routing** (true stereo). *Norm*: each side feeds itself, as on the original. *Cross*: each side's repeats come back on the other side, ping-pong. *Sum*: both feed both.
- **Low cut / High cut / Drive** act inside the loop, so they build up with every repeat: a high cut darkens each pass a little more, drive thickens the repeats and rounds off loud ones. *Off* / 0 % at their defaults.

**MUSICAL**
- **Snap** pulls the Pitch knobs onto a scale, counted from the unshifted note: chromatic, major, minor or pentatonic.
- **Sync** makes Delay and both crosspoints step through note values (1/64 up to a whole note, with triplets and dotted) at the project tempo. A note longer than the memory holds reads *> MAX* and plays at the longest length that fits.

**SCRUB** slides the crosspoint region back and forth, by up to its own length each way. *LFO* sways smoothly; *RND* wanders to a new spot each cycle. It needs a region smaller than the whole memory to have room to move.

**Each channel**
- **Mode** and **Latch** sit at the channel's outer edge, one set per side as on the hardware. *Delay* gives the channel a plain delay set by its Delay knob, with the treble lift. *Pitch* makes its heads move through the crosspoint region at the Pitch ratio. **Latch** (the Memory Latch) stops recording and keeps looping the crosspoint region, with pitch and reverse still active; it lights solid orange while latched. In Quasi-stereo there is one recording, so either latch holds both sides.
- **Delay:** the delay time in Delay mode. It fades when Pitch mode is on.
- **Pitch:** 0.25× to 2× (−24 to +12 semitones). Its ring lights from the centre, where the pitch is unchanged.
- **Fine:** ±100 cents on top of Pitch. The original has Coarse and Fine pitch pots; the ±100 cent span is a choice made for the clone.
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

Options: `--mode delay|pitch`, `--stereo true|quasi`, `--range long|short`, `--bandwidth 5|10|20`, `--delay`, `--pitch`, `--xp1`, `--xp2`, `--feedback`, `--mix`, `--vibrato`, `--vib-rate`, `--freeze`, `--freeze-after`, `--no-xing`. Modern: `--fine`, `--snap off|chromatic|major|minor|pentatonic`, `--vib-shape sine|square`, `--route normal|cross|sum`, `--lowcut`, `--highcut`, `--drive`, `--scrub`, `--scrub-rate`, `--scrub-mode lfo|random`. Every channel option sets both channels. Sync needs a host tempo and the keyboard needs MIDI, so both are plugin-only.

## Interface snapshot

```bash
build/plugin/the89th-snapshot out.png [width] [pitch|delay|freeze|quasi|modern|keys|kb]
```

Runs audio through the real plugin and renders the panel to a PNG, so you can check the interface without opening a DAW. The mockups the design came from are in [`docs/design/`](docs/design/).

## Layout

```
libs/core/     header-only DSP, no JUCE: converter, memory, read voices, Xing, filters, resampler, keyboard
plugin/        VST3 + Standalone shell, the panel (src/gui), tests, snapshot tool
cli/           WAV → WAV renderer
scripts/       validate.sh: pluginval over several seeds
tests/         Catch2 / ctest
docs/          plain-English overview, clone status, design notes, hardware research, mockups
```

Start with [`docs/what-this-is.md`](docs/what-this-is.md) for the plain-English picture.

## How close it is

[`docs/clone-status.md`](docs/clone-status.md) goes through every behaviour and marks each one as matching, derived from published figures, assumed, or not modelled.

- **Matches or derived:** the two-head crosspoint mechanism, reverse, the memory latch (per side), feedback through the pitch shifter, the clock-based bandwidth switch and its octave jumps, the 16,384 × 13-bit memory, true and quasi stereo, delay and pitch modes (per side), the treble lift and cut, the short range, the converter and its clip, word-by-word reading, the published band edges and delay limits, and delay changes without pitch bend or clicks.
- **Assumed:** the filter type, the treble-lift curve, how the signal-aware join (Xing) picks its join points, the crossfade curve, and the vibrato ranges. Each is a single named constant, easy to change.
- **Keyboard:** every section of the KB 2000's panel is modelled, from its panel and brochure: biphonic pitch, envelope, glissando, vibrato, Memory Synchro and Reverse Synchro. Their times and ranges are ours.
- **Not modelled:** the rear-panel control sockets (pitch clock, voltage control, insert loop), the KB 2000's microphone input, and the drift of the analog pitch oscillator.
- **Open question:** at some pitch ratios (×0.75, for one) word-by-word reading puts image tones in the audio band, about 4 % distortion on a sine. Publison's brochure quotes 0.2 % in pitch mode. See the clone status page.

Demo videos of real units have been checked (listed in the clone status page), which settled the panel layout and the keyboard. Nothing has been measured against a working unit yet; recordings from one would settle most of the remaining assumptions.

## Next

- Recordings from a working unit, or the service manual, to settle the assumed parts and the ×0.75 question.
- Tuning the KB 2000's times and ranges by ear, in real sessions.
- Optional: a slow drift of the pitch clock, as the original's analog oscillator has, off by default.

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
