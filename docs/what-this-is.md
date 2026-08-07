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

It also settles something for later. The original stored sound in an unusual format that keeps its error proportional to how loud the signal is, instead of fixed. When we build that, it needs to be 13 bits wide, not 16. Getting that wrong would have made everything slightly too clean.

---

## What is deliberately missing

This is a first pass. Four things are left out on purpose, with the hooks in place for each.

**The smart join.** The original works out where in the sound to make the jump, so the join lands somewhere the waveform already matches. Ours always joins after the same fixed interval. This is the hardest part of the whole machine to copy and the person who made the best-known modern version says he did not manage it either. Ours is clean, but it will not have the original's knack for landing the join musically.

**The filters.** Real converters need filters either side of them. Ours has none yet, and you can measure the result: at the 5 kHz setting a 440 Hz tone comes back with an extra tone at 12787 Hz nearly as loud as the note itself. On the original that region has a particular ringing quality. On ours it is currently just a wrong noise. Fix this before judging the low settings by ear.

**The storage format.** Sound currently goes into memory at full quality. The original's format is part of its character. Left for later.

**Vibrato, LFOs, MIDI, and a proper interface.** None present. The controls are JUCE's plain list for now.

Also worth knowing: there is no limiter anywhere. Push feedback up and the output can go well past full scale. The original ran into its own converter and clipped. Ours will just get loud.

---

## What you have

A VST3, installed and ready. Mac, Apple Silicon.

A command line renderer that takes a WAV in and writes a WAV out, so you can compare settings without opening a DAW.

Controls: Pitch, Crosspoint 1, Crosspoint 2, Feedback, Mix, Bandwidth, Freeze, Init.

31 automated tests. The ones that matter most:

- At pitch 1.0 the sound comes out the far end **identical**, bit for bit. Not close, identical. If anything in the delay path were subtly wrong this test would fail.
- A 440 Hz tone at pitch 2.0 comes out at 880 Hz, checked to within 1%.
- The join never produces a jump bigger than the sound could make on its own, checked across six pitch and direction combinations. There is a deliberately broken version in the tests that has to fail this check, so we know the check is capable of catching something.

Two of those tests found real bugs while this was being built. Both were in the crossfade, and both looked correct on paper.
