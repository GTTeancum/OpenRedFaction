# Precision weapons first pass

DEV slots6/7 select Sniper Rifle and rail_gun. Primary definitions, clips, inventory and correct fire/reload sound labels feed existing shared weapon flow. Sniper respects cover and chooses the nearest actor; rail passes world cover and publishes ordered multi-target damage. Actual scene helper checks use real body/registry/mover geometry with a damage recording seam; full live NPC resource/damage acceptance remains.

PC ordinary input replays each fire once: sniper loaded6->5, reserve36; rail loaded1->0, reserve10. Captured first-person views inspected after using inverse authored model-space offsets and FOV65/70. Sniper resource peak1,168,872 bytes required raising only slot6 cap to1,280KiB; rail peak1,033,384 remains below1MiB. Scope, scanner, trails, sniper material penetration and wider live combat remain unfinished.

Cyclic_Timer20 is also integrated: finite/unlimited pulses to events and movers, retained enable/disable count/deadline, normal activation propagation unchanged. Focused event_cycle/runtime_cycle checks pass. Cycle state is not checkpointed yet.

Initial native run render-20260918-081539 completed150 frames with matching weapon/ammo/combat words but harness failed TERRAIN_NOISE: both platforms correctly had all-zero lazy noise state because no destruction occurred. Harness now accepts that unallocated state while preserving exact platform equality and allocated-owner budget checks. It restored the disc and closed its emulator; rerun pending.

Native rerun render-20260918-084357 PASS: 77 comparisons,150 frames, 3346 available endpoint pages on stock64MiB, restored disc. Weapon/ammo/primary-shot state matches PC. This verifies resource residency and firing, not live scope/scanner or multi-NPC combat.
