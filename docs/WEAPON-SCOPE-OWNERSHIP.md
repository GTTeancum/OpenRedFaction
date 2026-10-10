# Player scope selection ownership

Status: integrated after independent source review against
`a21753560026cded820d98f43442cbe194746fa6`, 2026-10-10. Uncompiled and
runtime-unverified; independent source review found no blocking defect.
Parent owns the scheduled 07:00 Xbox batch.
No build, syntax check, test, fixture, emulator, campaign route or capture
was run for this slice.

## Original evidence and bounded policy

Evidence was read-only disassembly of `Installed_Game/RF.exe` (1.20 NA),
SHA-256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Both weapons' authored `alt_zoom` entries were checked in
`Installed_Game/tables.vpp` / `weapons.tbl`; original inputs were not modified.

The original RF.exe selection path retires local zoom for an admitted
selection: `4a4d96 -> 4aa0b0`, then `4aa0c0 -> 4ad8a0`. Stores at
`4ad8a6..4ad8b2` clear player `+f94`, `+f95` and `+f98`. The zoom getter
`4ace79` reads `+f98`; the reticle path consumes that result at
`43a402..43a415`.

Selection is not an unconditional reset. `4a4cad..4a4cb3` rejects an identical
pending request; `4a4cc9..4a4cde` avoids a new ordinary nonforced selection
when the current primary already matches. A forced same-weapon selection
can proceed farther. This patch preserves same-qualified-owner state and
does not reconstruct that forced-selection distinction.

The original has additional alternate-input latch/timer logic at
`4a638c..4a63bc`. It does not establish that a newly pressed alternate must
be discarded when selecting another weapon. Ordinary reconciliation therefore
preserves the port's existing alternate latch: an already-held button cannot
retoggle, while a fresh simultaneous selection/alternate edge can toggle the
new owner. There is still only one `rf_weapon_scope_step` per combat tick.

The existing shared two-state 90-degree ordinary / 25-degree scoped policy,
projection math and inverse-projection look scale are unchanged. These remain
labelled port defaults, not original variable-magnification reconstruction.

## Missing consumer and implementation

Previously, both Sniper slot 6 and Precision Rifle slot 15 supplied the same
true `selected` boolean to the scope step. Switching directly between them
could retain the old active zoom and reduced look scale indefinitely.

`scene_scope_owner.inc` adds a 12-byte transient owner: full generation-bearing
player handle, selected slot and actual selected weapon ID. A qualified scoped
owner must have consistent object-registry/entity-registry, object view, entity
view and damage handles; a retained body; finite positive health; valid owned
catalog identity; ordinary on-foot control; and no explicit-unarmed or
retired/hidden object state. Failed qualification clears scope instead of
creating a replacement authority.

Two live observation points close the selection gap:

- Immediately before the existing scope input step, after pickup, cycle and
  mode input. A changed/lost owner clears active zoom and ordinary look and
  projection factors; the one normal step then owns the current alternate edge.
- After successful combat and both conventional/machine depletion consumers,
  before presentation. This catches selection after the scope step, including
  grenade depletion, Remote follow-up and conventional/Fusion/Flame/MP fallback.
  It only reconciles ownership and never advances input a second time.

No selector callsite or selection side effect changes. Ordinary armed/unarmed
pickup selection and cycling are observed before scope input. Raw fallback,
script/form changes and identity loss are observed at the next existing live
reconciliation point. The patch does not mutate shared weapon selection, ammo,
camera shake, raw gameplay aim, other alternate modes, rendering formulas or
save/checkpoint layouts.

## Failed-load and lifecycle boundaries

`campaign_select_primary` remains raw. In particular these provisional or
restore-shared callers remain untouched:

- `scene_world_load.inc`: selected-player publication, before later fallible
  world publication/storage-close work.
- `scene_remote_checkpoint.inc`: both world and frame-zero restore paths.
- `campaign_player_import_apply`: later disguise validation can still fail.
- `campaign_ammo_reset`: later catalog/admission work can still fail.
- `campaign_ammo_publish`: fallback selection is shared with restoration.
- Existing development/fixture selectors and Machine Pistol mode publication.

A successful world load explicitly retires scope only in the existing
`!status && world_published` block after storage close. It clears active zoom,
forgets the old owner and consumes the currently held alternate, including
when the restored player/weapon tuple matches the old one. Preparation,
rejection, absent loads and storage-close failure do not invoke this reset.
This is a transient policy; no scope state is serialized.

Frame-zero initialization uses alternate-held zero exactly as before, so a
real initial alternate press still works. Death, loss of ownership, unarmed
state, mounted control and handle invalidation clear scope when observed by
the live reconciliation; the existing scene/restart and camera-before-combat
ordering is not moved. Intermediate owner changes that return to the same
qualified tuple between observations are not inferred as forced reselection.

Ordinary scope retirement restores its look factor and projection factor to
one. If a cinematic FOV owner is active, retirement leaves the published
cinematic projection alone. Normal combat still precedes the existing authored
FOV override; successful end-frame load retirement can also occur after it.

An ownership-driven clear happens outside the shared scope step, so the existing
`SCOPE` trace's `changed` result does not report that clear. The trace still
reports normal alternate-input toggles; no new telemetry/observer is added.

## Review and validation limits

Source review covers declaration/include order, the one input-step owner,
pre-input and post-selection ordering, all raw selector callers, bounded catalog
indices, frame-zero fresh edges, held-button transitions, same-owner no-ops,
cinematic projection ownership and the post-close successful-load guard.
No runtime behavior or stock-64-MiB admission is claimed by this source slice.
