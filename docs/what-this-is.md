# What this is, in plain English

A plugin called THE89TH. It copies a French studio box from 1978 that shifted pitch and made delays.

This page explains what got built and why it should behave like the real thing. No maths.

---

## What the original box did

Think of a tape loop.

One head records onto the tape. Two other heads play it back. The recording head runs at a fixed speed. The playback heads can run at any speed you like.

That is the whole trick. Play back faster than you recorded and the sound goes up in pitch. Play back slower and it goes down. There is no clever processing anywhere, just two heads moving at the wrong speed on purpose.

The catch: tape runs out. A playback head moving faster than the recorder eventually catches up to it, and one moving slower falls off the back. So the box gives you two markers, called crosspoints, that fence off a stretch of the tape. When a playback head hits one marker it jumps back to the other and carries on.

That jump would click. So the box has two playback heads instead of one, and fades between them across the jump. One head is fading out as the other fades in, and you never hear the join.

The 1978 machine did all of this with about 200 logic chips and no computer in it at all.

---

## What we built

The same thing, in software.

A block of memory stands in for the tape. A write pointer walks through it at a steady rate. Two read pointers walk through it at whatever rate the Pitch knob says. Two crosspoint controls fence off the region. When a read pointer reaches the far fence it jumps to the near one, and the two pointers crossfade across the jump.

That is not a description of the sound we were aiming at. It is a description of the code.

---

## Why it should sound close

Four things fall out of building it this way instead of faking the result.

### Reverse is not a mode

On the original, reverse is not a button. You get it by setting crosspoint 1 deeper than crosspoint 2, which makes the read pointer travel the other way through memory.

Ours works the same way. There is no reverse switch and no separate reverse code. Swap the two crosspoints and the pointer runs backwards, because the direction is worked out from which crosspoint is which.

This matters because reverse then combines with everything else for free. Reverse plus pitch up, reverse plus freeze, reverse plus feedback: none of those are special cases anybody wrote. They just happen. On a box where reverse was bolted on as a mode, some of those combinations would not work, or would work differently.

### The bandwidth switch jumps the pitch, and that is correct

The original has a 5 / 10 / 20 kHz switch. It sounds like a tone control. It is not. It changes how fast the machine records and plays back, which is why narrower settings give you longer delays.

Flip it while sound is in memory and the pitch of what is already stored jumps by an octave, because the same recording is now being replayed at a different speed. On the hardware that is a side effect nobody designed. Players use it deliberately.

Ours does the same, and for the same reason rather than by imitation. Flipping the switch changes the clock and touches nothing else. What is already in memory stays exactly where it is and gets replayed at the new speed. There is a test that flips the switch and checks that not one value in memory moved.

We could have faked this by detecting the switch and shifting the pitch. We did not, and a faked version would come apart the moment you combined it with freeze or reverse.

### Freeze loops instead of freezing

Freeze on the original stops recording but keeps playing. So the fenced-off region loops round and round, and you can still pitch it and reverse it. It is a sampler, years before that word meant anything.

Ours stops the write pointer and lets the read pointers carry on. Because pitch and direction were never tied to recording, they keep working on the frozen sound with no extra code.

There is a small detail here that is easy to get wrong. If you stop the recorder and the playback head is running at exactly the same speed, both are still and you get one sample held forever, a dead tone. The real machine loops. Ours works out the read speed relative to the recorder, so when the recorder stops the read pointer starts moving through memory on its own. Freeze loops the region, as it should.

### Feedback runs through the pitch shifter

Feedback on most delays repeats the same sound quieter each time. Here the repeats go back through the pitch shifter, so each one comes back shifted again: up, up, up, or down, down, down. It arpeggiates. That is one of the sounds the box is known for.

Ours takes the feedback from after the pitch shift, so the repeats stack up the same way.

---

## The one number worth knowing

The research said the machine had about 210,000 bits of memory, and quoted maximum delays of 300, 600 and 1200 milliseconds.

There are two ways to read that. If each stored sample is 16 bits, you get 13,312 samples of memory. If the chips are wired the way that kind of chip is normally wired, you get 16,384 samples of about 13 bits each.

Only one of those matches the published delay times. 16,384 lands on 300, 600 and 1200 almost exactly. 13,312 misses every one of them by about 17%.

So the memory is 16,384 slots, and each slot is roughly 13 bits, not 16. That number is now baked into the plugin, and it sets the delay times you get.

It also settled the storage format. The original stored sound in an unusual format that keeps its error proportional to how loud the signal is, instead of fixed. It had to fit in 13 bits. There are only a few ways to split 13 bits into a number and a scale, and only one of them gives the 95 dB range the specs quote: a 9-bit number and a 3-bit scale. That's what the plugin uses now. It means every sample keeps about the same small grain whether it's loud or quiet, and that grain is part of the sound.

---

## What got added to finish the clone

**Two modes.** The original has a switch for Delay or Pitch, and a Delay knob on each channel. In Delay mode each channel is a plain delay. It also adds a treble lift before storing and takes it back off afterwards, which pushes the storage noise down. In Pitch mode the two playback heads move through the crosspoint region, and the treble lift is switched off. It's switched off because a pitch-shifted replay would move the lift and leave the treble wrong. Phase 0 only had Pitch mode.

**Two stereo layouts.** In true stereo, each channel gets half the memory and half the converter, so it can't reach 20 kHz. In quasi-stereo, one input fills the whole memory at full speed, both channels read from it with their own pitch and crosspoints, and 20 kHz works. With only one input being recorded, the two outputs are mixed together before they go back in as feedback.

**The storage format.** Described above: 13 bits, with an error that follows the level. It also clips at full scale, so pushing feedback can no longer run away. It folds like the original's converter did.

**Word-by-word reading.** The original reads memory with counters and no arithmetic, so a moving playback head just takes whichever stored word it lands on. It repeats some and skips others. Phase 0 blended neighbouring words together, which is cleaner than the original and lost treble at the top of each band. Now it reads word by word.

**The filters.** The original has filters either side of its converter. We added them. At the 5 kHz setting, the false tone at 12.8 kHz that Phase 0 made nearly as loud as the note is now over 150 dB down. Each band reaches its published −3 dB point at 5, 10 or 20 kHz.

**The smart join.** The original picks where to join so the waveform lines up. Ours now does too. At each join it searches for the spot where the stored sound best matches what's playing, and jumps there. On a steady tone the join becomes inaudible: even a join with no fade at all makes no click. Nobody has published how the original circuit did this. So ours copies what it does, not how. It's the part most likely to sound different from a real one.

**The short range and vibrato.** Later units had a switch that divides the delay range by ten, for doubling and flanging. They also had vibrato depth and speed per channel. Both are in.

**What's still missing:** the rear-panel control sockets and the keyboard that plugged into them. They control the machine; they aren't part of its sound. Some details aren't published anywhere, like the exact filter shape, the treble-lift curve and the vibrato ranges. For those we made reasoned choices and wrote them down. [`clone-status.md`](clone-status.md) lists each one.

---

## What got added on top

Once the clone was done, modern controls went on top. None of them is on the original, and each does nothing at its default, so a fresh instance still behaves exactly like the 1978 box. You can add one modern feature at a time rather than switching the whole thing into a "modern mode".

**Presets and A/B.** Fourteen factory sounds to start from, and your own saved as files. A/B holds two settings so you can flip between them while you tweak. Both are saved with the FL project.

**Where the repeats go.** Normally each side feeds its own repeats back into itself, as on the original. *Cross* sends each side's repeats to the other side, so they ping-pong. *Sum* sends both to both.

**Colour in the loop.** A low cut, a high cut and drive sit inside the feedback loop, so they act again on every repeat. A high cut makes each repeat a little darker than the last, like tape echoes. Drive thickens the repeats and rounds off the loud ones, which also stops a loop at full feedback from running away.

**Musical pitch.** *Snap* pulls the pitch knobs onto a scale, so a harmony lands on a real note. *Fine* nudges each side by up to a semitone, for detuned doubling.

**Tempo sync.** Delay and both crosspoints step through note values at FL's tempo instead of milliseconds. The memory is short (about a third of a second per side at 10 kHz), so long notes don't fit; they play at the longest length that does and the panel says *> MAX*. That shortness is part of the machine, so it isn't hidden.

**Scrub.** Moves the looping region back and forth through memory by itself, smoothly or at random. On a frozen loop it turns one moment of sound into a moving texture.

**Square vibrato.** In pitch mode it jumps up by the depth and back, a trill.

**Link.** Channel 2 copies every channel 1 control, so you set one side and get both.

## What you have

A VST3, installed and ready. Mac, Apple Silicon. A Standalone app, and a command-line renderer that turns a WAV into a WAV so you can compare settings without opening a DAW.

The original's controls are all there: the global switches Mode, Stereo, Range, Bandwidth, Freeze and Mix, and per channel Delay, Pitch, both Crosspoints, Feedback and Vibrato. Init resets everything. Delay and crosspoints read in milliseconds, or in note values under Sync. The modern controls above sit in their own row and in each channel, and the preset strip runs along the top.

## What the panel looks like, and why

The panel is black and orange. Orange marks anything live: a value, a lit ring, an active switch. Everything else stays out of the way. It went through three looks to get here. The first was a generic dark app with coloured rings, which read as software, not an instrument. The second was 70s hardware with wood sides and metal knobs, which read as too vintage. This one is flat and digital, taken from mockups generated with Higgsfield and then drawn in code so it stays sharp at any window size. The mockups are in [`design/`](design/).

What's worth knowing when you use it:

- **The displays show the machine working.** Each channel's memory is drawn as a circle. A white tick is the recording head sweeping round. The orange arc is the crosspoint region, and the orange squares are the playback heads, as bright as they are loud. At a join you can watch one fade out while the other fades in. None of this was visible on the original.
- **Every knob shows its value underneath, in orange.** The display's bottom line also echoes whichever knob you're turning.
- **Controls that do nothing in the current mode fade** instead of disappearing, so nothing moves under your hand. In Pitch mode the Delay knob fades. In Delay mode, Pitch and the crosspoints fade, unless Freeze is on, because a frozen loop still uses them.
- **Freeze lights solid orange** while it's holding the loop.

99 automated tests, and a stress test in a simulated host (pluginval) at its strictest level. The ones that matter most:

- At pitch 1.0, and in delay mode, the sound comes out **identical**, bit for bit, apart from the storage format's own grain. The tests check that too.
- A 440 Hz tone at pitch 2.0 comes out at 880 Hz, to within 1%.
- A click sent through the whole plugin comes out exactly when the delay setting says it should. This test caught a bug that had been there since Phase 0: for the first pass through memory, part of the output was a whole memory's length late. A steady tone hid it completely.
- The false tone at 5 kHz bandwidth is over 150 dB down, and each band edge lands within 1 dB of its published point.
- With the smart join on, the level stays flat through every join on a steady tone.
- With every modern control at its default, the machine behaves exactly as before they existed: all the earlier tests pass untouched.
- A saved project comes back byte for byte, presets and A/B included.

Several of these tests found real bugs while this was being built, and each bug looked correct on paper.
