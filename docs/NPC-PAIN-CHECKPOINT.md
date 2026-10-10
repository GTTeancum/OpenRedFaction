# NPC damage-reaction ordinary saves

Source written 2026-10-09 against `efa4ec7f`. This slice has not been built or
run. The parent integrated the separate capture/restore hooks at14:50UTC;
no new fixture, emulator run, campaign traversal or original-asset change occurred.

## Practical blocker and source ownership

Ordinary firearm hits call `combat_notify`, which uses the retained
`rf_scene_npc_pain_retained` adapter and original `0x428740` orchestration.
`rf_entity_pain_react` chooses mapped action22 or23, starts its actual clip,
sets the existing1000–2000ms cooldown and derives the attack lock from the
authored duration plus250ms. The ordinary enemy attack loop checks that lock.
These are existing gameplay rules, not new stun timings or damage behavior.

`scene_npc_checkpoint_row_profile` previously rejected every pending pain
AI/lock/cooldown timer and every unowned non-looping flinch clip. Its only pain
exception was an active burn whose RFAP4 sidecar already retained the timers.
Consequently a routine nonfatal firearm hit could make the whole-world save
temporarily unavailable. Existing L4S5 actor3270/3305 retained damage evidence
and the actual attack-lock consumer are documented in LIVE-DAMAGE-REACTIONS.md.
The previous reactive-save check deliberately saved before the first hit;
NPC-REACTIVE-SAVES.md does not establish in-progress pain continuation.

## RFNC15 and bounded integration

- Preserve RFNC1–14 decoding and unchanged version selection when no pain
  owner needs continuation. A quiet frozen owner can still emit RFNC14.
- Append a24-byte pain tail after RFNC14's physics tail on each RFNC15 row:
  presence, three relative deadlines, selected action and shared pain RNG.
  A component containing one affected row uses RFNC15 for all its rows.
  Absent tails are zero. The maximum full row is1604bytes; the existing total
  save-capacity and staged-memory budgets still apply and have not been raised.
- Preserve disabled timers as-1 and expired timers as0. A disabled timer or
  retained selected action is meaningful even after its clip has completed.
  No-pain legacy/absent loads use expired deadlines at the restore clock,
  rather than-1, which would prevent the next pain cooldown from expiring.
- Admit only living, registered, non-burning ordinary pain and actions-1/22/23.
  An active non-looping flinch must match the real class/weapon motion mapping.
  Existing full playback, resource demand and candidate-world placement remain
  responsible for pose continuation. No hit or flinch-start callback is replayed.
- Stage rebased deadlines before publication, then assign timers, selected
  action and RNG alongside the already staged NPC playback. All RFNC pain rows
  must agree on their shared RNG. RFAP4 remains the sole burning-pain owner;
  mixed saves must agree on that RNG and cannot claim the same actor twice.

The isolated implementation is `scene_npc_pain_checkpoint.inc`; the core
header/codec carry the format. Parent hooks include it from the existing NPC
capture adapter, extend the restore entry with three prepared deadlines, and
reconcile RFAP ownership. No `scene.c` edit is needed. Read-only
`rf_scene_npc_pain_restored` telemetry reports assignments, last UID, presence,
remaining deadlines, selected action and RNG for the scheduled native check.

## Boundaries and pending verification

No claim is made that every moving combat state can now save: existing actor
velocity, weapon firing, unholster/death, unsupported attachment and global
effect admission still apply. Active pain audio remains presentation state;
loading does not restart it. This change preserves the gameplay attack lock
and clip clock rather than promising exact continuation of an audible voice.

The parent-owned batch still needs compilation and one bounded real nonfatal-hit
save/fresh-load continuation showing unchanged vitals/ammo, the saved clip and
remaining attack lock, no repeated damage/audio request, and resumed ordinary
combat after expiry. Existing RFNC14 frozen-save compatibility must remain
intact. Existing capture-test expectations that all pending locks reject are
stale for this new supported profile; no tests were run in this source slice.

Overall working-alpha estimate remains roughly90%; no increase is earned by
this source-only persistence extension.
