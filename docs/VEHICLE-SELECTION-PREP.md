# Data-driven vehicle selection preparation

Boot-time authored selection is now integrated in `scene_vehicle_selection.inc`; runtime host switching remains deferred.

`campaign_spawn_prepare` selects eight hardcoded active hosts: L1S2 Driller8122, L1S3 APC9627, L5S3 sub3963, L5S4 sub3955, L10S3 sub6794, L12S1 Jeep7629, L13S3 Fighter8955, L18S2 Fighter10066. Other supported instances become passive owners in `campaign_bind_passive_vehicles` (sub/Fighter01/masako_fighter/Driller01/APC/Jeep01).

The active host registers an entity view; passive owners register only object-kind11. Existing registry APIs cannot replace an owner while preserving its generation/handle. Blindly swapping pointers breaks event/target/group identity. Active chassis/cockpit/weapon resources load once; passive owners only have chassis resources, so arbitrary cross-class promotion also needs a stock64MiB resource transaction.

RFVC has profile/physics/ammo but no stable active-host UID. RFVA preserves passive pose/vitals/attachment, not parked active ammo/aim/route. Naive promotion resets gameplay state and changes which host an ordinary save restores.

Small initial implementation: parse supported authored entities and choose an eligible ungrouped host by distance to player spawn, with stable UID tie-breaking and authored NPC seat-host priority. Preserve group references and hidden metadata. Add a stable UID/profile selection wrapper read before resources load; retain the eight legacy mappings only as old-save fallback. This removes the level allowlist while keeping one active host and current memory scope. It does not solve choosing a later unhidden host through Use.

Nearest-Use switching requires generation-preserving owner registration, per-owner parked physics/ammo/aim/route state, checkpoint host identity and explicit group detach rules. Start with quiescent same-class promotion to avoid cross-class resource swaps; ordinary moving or attached promotion remains a separate bounded task. Do not claim full multi-host gameplay from boot selection alone.


## Integrated boot selection

Existing eight level/UID/class choices remain preferred, including the intentionally hidden L1S3 APC. Other levels scan supported Driller01/APC/Jeep01/sub records, using the original spawn and vitals readers. Hidden, explicitly dead, seated-child and first-list group-attached candidates are excluded. Choose the smallest squared distance to authored player spawn, breaking ties by stable UID. Resource use remains one active vehicle profile. No host promotion or registry replacement occurs.

The selector streams entity/group metadata and uses at most64KiB of temporary raw-record storage, freed per candidate. Class/pose validation and selected-UID duplicate detection use existing readers. Telemetry records eligibility/rejection counts and the selection source. Neither friendliness nor absence of an NPC driver is invented as a universal player-use rule.

New Fighter01 selection is deferred because existing metadata alone does not distinguish intended manual-use hosts from combat Fighters; the two known active Fighter hosts remain supported. masako_fighter also needs its own active resource binding. Later-unhidden vehicles and switching between multiple usable hosts remain open.

Ordinary save source identity pins authored level bytes, so new deterministic selection repeats on fresh load. The eight existing choices preserve their previous host identities. A legacy passive-only world save for a newly activated section is rejected before publication; migration is not implemented. The loader does not silently reinterpret a passive owner as an active saved host.

Installed authored-data audit: the implemented fallback selects L10S1 Jeep01 UID2485, L10S4 sub UID7010 (Jeep7009 is farther from spawn), and L15S1 APC UID3303. Together with the preserved eight choices, eleven SP sections now have a selected active host. This is an authored eligibility audit, not runtime verification of those three sections.

Xbox result: `artifacts/xemu/vehicle-selection-20261003-163818/report.json` PASS,180 frames,stock64MiB,2864 free physical pages. Exact grounded Jeep/miner fixture bytes were loaded under ctf06.rfl rather than a legacy section name, with no vehicle-profile override. Metadata selected UID7629/profile3/fallback2; ordinary Use boarded the player as gunner alongside the living NPC driver, and the authored route moved the chassis2.2872m horizontally. NXDK build and disc restoration pass. Other vehicle profiles, multiple-candidate rejection order, save migration, transitions and audiovisual behavior remain unverified.
