# Authored Switch ambient targets

Source-written on 2026-10-09 against `633a8a4e` and integrated after the16:00
batch in `0cabeeb0`. It has not yet been compiled or exercised in a gameplay run;
the parent owns the17:00 consolidated Xbox batch.

## Concrete remaining authored gap

A fresh, read-only parse of the four original level archives reproduces all
83 Switches and119 links. The13 links previously described as unsupported in
LIVE-SWITCHES.md actually comprise9 controller links,3 ambient sounds and1 camera
entity. The current controller adapter already reaches all9 controller links,
including L19S3 Switch12024's Big Door9311/9312. This change adds no new controller,
camera, light, NPC or vehicle behavior.

The remaining sound targets are all in original `levels2.vpp/L7S1.rfl`:

- Switch3685 links ambient4062 at `(34.25, -1, 25.75)`.
- Switch3714 links ambient4064 at `(14.25, -1, 15.75)`.
- Switch3916 links ambient4063 at `(43.4173279, -1, -17.4706459)`.

Each sound is `Amb_Energy_02.wav`, near distance5, authored volume0.3,
rolloff2, and startup delay0. They are the first three ambient records. Each
Switch starts enabled, has zero event delay, and retains its ordinary authored
mode/unlimited/activation fields. Each sound's only incoming event or trigger
link is its named Switch. Switch3685 is reached by Trigger3690; Switch3714 by
Trigger3713 and Delay3726; Switch3916 by Trigger3992. None of these Switches has
an incoming Remove_Object/type2 link. Type3 is Invert, not a removal action.

These are data/source findings, not a campaign traversal or native-runtime
claim. The separately retained inventory is a development artifact outside the
checkout; original game inputs were read without modification.

## Original behavior and live ownership

The verified original executable SHA-256 is
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
`4bc340` checks ambient lookup`45afe0` after triggers/controllers and before
lights/events/general objects. Ambient OFF`45b010` and ON`45b040` call`505b50`
with the existing ambient slot and zero/authored volume. They do not change an
enabled flag or allocate, free, stop or restart a voice. The reconstructed
`rf_ambient_slot_volume` already implements that write; see AMBIENT_SOUNDS.md
and FORCE_REGIONS.md for the retained original-function evidence.

The new scene consumer uses that existing slot service. A slot must still be
in range and name its instance's sample; missing or mismatched slots are no-ops.
Normal ambient processing remains responsible for spatial gain, threshold-based
voice stop/start, PCM admission and independently generation-qualified device
IDs. Other slots and the shared sample metadata are untouched.

Sound links retain authored UIDs, while registered object/controller links use
generation-bearing handles. The runtime Switch wrapper now carries the exact
current link's kind into sound lookup. A resolved or stale handle cannot become
a sound UID merely because its numeric value matches. All other family ordering,
ordinary linked-event dispatch, source/actor forwarding and delay/limit handling
remain unchanged.

## Bounded state continuation

The adapter admits a sound only when it has one retained instance, zero authored
startup delay, one initially enabled controlling Switch, and no competing
event/trigger link. A Remove_Object link to that Switch also excludes it because
section history does not retain the final Switch state for that lifecycle. These
are ownership criteria, with no hardcoded level names, sound names or UIDs.

The existing startup order is preserved: initial Switch links run before ambient
slot allocation, so writing an absent slot is still a no-op. No latent mute flag
is added. All admitted initial Switch states are enabled, so immediate ambient
allocation retains the same authored starting volume. Live activation writes only
the existing slot, through the normal delayed Switch dispatcher.

RFEC already saves each Switch's disabled/count/limit/mode state. At the end of a
successful ordinary world-load publication, a one-shot assignment-only repair
reconstructs these uniquely owned slot volumes from the restored Switch states.
It does not re-fire any event, consume an activation, play a Switch sample, change
an ambient deadline, or reset a PCM cursor. Rejected private staging does not run
this repair. Section revisits reuse their existing Switch-state reapplication
after ambient scheduling; no new history format or retained handle is introduced.

There is no per-frame projection, so the adapter adds no recurring ambient-sweep
cost or permanent authority over future sound writers. Delayed sounds,
initially-disabled Switches, shared sound writers and removable Switches
remain unsupported. Full ambient timing/PCM-cursor persistence remains separate;
RFAS is a terrain identity/source format and is not used as audio serialization.

## Observation and next parent batch

`rf_scene_switch_ambient[8]` reports successful lookups, dispatch requests,
qualified slot writes, absent/mismatched slots, restore writes, last sound UID,
last slot and last volume bits. Existing ambient playback and event counters
continue to describe the actual consumers. Initialization can increase the
no-slot counter without writing or playing anything.

The next parent-owned batch will make one bounded ordinary-input attempt at
original L7S1 Trigger5504, whose authored cutscene reaches Switch3714 and sound4064.
It can establish that one OFF write while observing unchanged4062/4063 volumes.
It cannot establish all three switches, ON behavior or saved-volume restoration;
those remain unverified. The source patch itself was prepared without any build,
emulator run or generated gameplay fixture.
