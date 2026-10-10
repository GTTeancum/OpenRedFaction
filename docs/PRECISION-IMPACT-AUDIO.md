# Precision impact audio

Integrated after the completed 23:00 batch on October 9, 2026. Source-reviewed;
awaiting the next hourly Xbox compilation. No precision runtime or audio output
is claimed.

The patch adds optional dry material/default audio at the existing accepted
precision contacts: detached piece, sniper prop, each rail prop, vehicle/turret,
and qualified NPC body. It also adds sniper-only world feedback at existing
blocked branches and the no-object selection path. Rail world-surface audio is
deferred because the scene does not retain its pierced world-contact sequence.

Weapon identity is captured once for a precision shot and passed into the two
internal clutter consumers. Known NPC collision material is retained; the
existing explicit Default fallback is used for other object contacts. All
audio calls are optional and cannot replace a damage/obstruction result.

Candidate gathering, fractions, tie order, rail penetration, one fragment,
per-prop visitation, one vehicle candidate, NPC hit capacity/order, shield
continuation, nano rail termination, damage amounts, ammo and save formats are
unchanged. World feedback uses the existing ordered retained-fraction helper;
it makes no generic closest-surface claim.

## Qualified contact ownership

Sniper clutter `handled=1` includes stale and blocked owners. Its impact is
therefore emitted inside the already qualified and unobstructed damage branch,
not from the caller's handled result. The blocked branch can request world
feedback; a stale owner emits neither a prop impact nor a replacement hit.

Vehicle/turret damage also sets `handled=1` before stale-owner and source
rejections. The added read-only `scene_driller_projectile_contact_live` checks
the retained tag/full handle against the live registered owner and existing
selection eligibility. Both precision and ordinary player hitscan capture this
audio-only qualification immediately before damage. The existing damage call
and return behavior are unchanged. Playback requires successful damage dispatch,
handled, and that pre-damage qualification, but not positive applied damage.
Capturing before damage preserves a genuine lethal impact if its callback
retires the target. The normal hitscan correction is included at parent request.

## Original evidence

Read-only disassembly of owned `Installed_Game/RF.exe`, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `4c504d` calls world penetration policy `4c9bf0`; its piercing path still
  reaches contact presentation `4c511b -> 4c5820`. Material/default sample
  selection at `4c5912..4c597f` requests positional playback at `4c5996`.
- `4c600f` calls object penetration policy `4c9b20`. Direct damage follows at
  `4c6132`; ordinary object contact selects a dry material group at
  `4c6463..4c6471`, falls back when its sample is -1 at `4c64d9..4c64eb`, and
  requests positional playback at `4c6509`. The continuation result is returned
  afterward at `4c655d`. Contact audio is therefore not conditional on stopping
  penetration or positive applied damage.
- Nano interception exits earlier at `4c5b54`, before that body-contact audio.
- Installed `tables.vpp/weapons.tbl` gives both Sniper Rifle and rail_gun dry
  Default/Flesh/Metal/Rock/Water/Glass groups and `$Piercing: true`. Rail also
  authors `pierces_all`; its Water group is Medium Water Splash.

This implements feedback for the port's existing first-playable contacts. It
does not reconstruct sniper material penetration, rail world-contact
enumeration, underwater projectile ownership, or special shield sounds.

