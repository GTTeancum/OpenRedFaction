# Dropped-weapon pickup feedback

Source-written on 2026-10-08 for the parent 22:00 Xbox batch. No build, test,
emulator run, gameplay fixture, or image was performed for this patch.

## Missing live behavior

`campaign_weapon_drops_tick` already grants a dead NPC's held weapon/ammunition
and retires its persistent drop exactly once. Unlike placed-item collection, it
did not publish a pickup notice or request pickup audio. The new call reaches
the existing `campaign_pickup_sound` and `campaign_pickup_notice_grant` only after
`grant.acquired || grant.rounds` and after changing the drop from available to
collected. It uses the drop's world position and its corpse owner's persistent
UID. Grant quantities, selection publication, range/cover checks, lifetime,
persistence and inventory-full rejection retain their existing code paths.

Original pickup completion at RF.exe45977c..45995c calls 459520 after successful
grant/retirement; 459520 resolves sound through 4594f0 and starts a spatial voice
at the item with gain1. The existing reconstructed callback retains the authored
class override, or weapon pickup sample12, and its bounded audio-bank admission.
Audio failure never rolls back inventory or reopens a retired drop.

## Authored class metadata

The existing pickup resource array owns space for every supported class, but
previously loaded definitions only for classes placed in the current level.
The optional metadata pass now fills missing definitions for the 15 supported
drop presentations, without loading models/textures or changing weapon demand.
It reads tables only during level loading; no pickup-time archive access or new
retained allocation is introduced. Per-class metadata errors leave that
definition absent and do not prevent gameplay; the standard weapon sound is
still requested, with no fabricated notice.

Bindings use installed `items.tbl` names and the existing scene slot catalog:
Handgun, Assault Rifle, Riot Stick, Shotgun, rocket launcher, grenades, Sniper
Rifle, rail gun, Remote Charge, flamethrower, shoulder cannon, Machine Pistol,
heavy machine gun, scope assault rifle and Silenced 12mm Handgun. Undercover's
`weapons.tbl` display name is Silenced 12mm Pistol; the silenced item supplies
only that authored presentation. Its `$Gives Weapon` points at the base handgun,
so this field is deliberately never used to grant or remap the existing
Undercover death drop. Remote Charge uses the single-charge item presentation.

The notice retains the existing new-ownership/ammunition template selection.
For baton charge and flamethrower fuel, the source-reconstructed
`rf_weapon_pickup_amount` display result converts hundredths to whole
batteries/canisters; its grant result is discarded. Accepted ammunition is never
scaled or increased. Empty acquired weapons still produce their ordinary
weapon notice. Existing placed-item formatting remains outside this patch.

## Observability and limits

`rf_scene_weapon_drop_feedback[8]` exports metadata loaded, metadata failures,
last metadata error, accepted feedback calls, notice requests, sound requests,
last weapon ID and last corpse UID. Normal installed admission is 15 definitions,
zero metadata failures; no pickup happens merely by loading these definitions.
The existing `rf_scene_weapon_drops`, `rf_scene_pickup_audio` and
`rf_scene_pickup_notice` remain the authoritative collection/audio/text outputs.

Rejected/full, occluded, distant, already-collected and dead-player paths do not
reach either callback. The existing single pickup-notice lane still shows the
latest accepted item when multiple drops are collected in one tick. Native
drop-notice display and audible playback remain pending the parent batch.
