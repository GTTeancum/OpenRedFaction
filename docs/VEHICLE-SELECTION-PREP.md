# Data-driven vehicle selection preparation

Deferred design only; no runtime selection change implemented. Scripted vehicle freeze/wake took priority.

`campaign_spawn_prepare` selects eight hardcoded active hosts: L1S2 Driller8122, L1S3 APC9627, L5S3 sub3963, L5S4 sub3955, L10S3 sub6794, L12S1 Jeep7629, L13S3 Fighter8955, L18S2 Fighter10066. Other supported instances become passive owners in `campaign_bind_passive_vehicles` (sub/Fighter01/masako_fighter/Driller01/APC/Jeep01).

The active host registers an entity view; passive owners register only object-kind11. Existing registry APIs cannot replace an owner while preserving its generation/handle. Blindly swapping pointers breaks event/target/group identity. Active chassis/cockpit/weapon resources load once; passive owners only have chassis resources, so arbitrary cross-class promotion also needs a stock64MiB resource transaction.

RFVC has profile/physics/ammo but no stable active-host UID. RFVA preserves passive pose/vitals/attachment, not parked active ammo/aim/route. Naive promotion resets gameplay state and changes which host an ordinary save restores.

Small initial implementation: parse supported authored entities and choose an eligible ungrouped host by distance to player spawn, with stable UID tie-breaking and authored NPC seat-host priority. Preserve group references and hidden metadata. Add a stable UID/profile selection wrapper read before resources load; retain the eight legacy mappings only as old-save fallback. This removes the level allowlist while keeping one active host and current memory scope. It does not solve choosing a later unhidden host through Use.

Nearest-Use switching requires generation-preserving owner registration, per-owner parked physics/ammo/aim/route state, checkpoint host identity and explicit group detach rules. Start with quiescent same-class promotion to avoid cross-class resource swaps; ordinary moving or attached promotion remains a separate bounded task. Do not claim full multi-host gameplay from boot selection alone.
