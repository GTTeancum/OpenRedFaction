# APC first playable integration

Select `RF_REPLAY_VEHICLE=apc` on PC or `--vehicle-test --vehicle-class apc` in the XEMU harness. The Driller selector remains available. `tools/check_apc_vehicle_replay.py` generates approach, board, drive, fire and exit inputs; `--frames 220` ends seated after firing.

Actual class data supplies six collision spheres, four springs, 5000 health, mass 10000, speed 6.5 and acceleration 3. Chassis, textures, driver seat and cockpit come from installed assets. Generic resource and ground physics adapters preserve existing Driller ownership.

APC.vfx has 86 meshes and 814 local references to 32 shared materials. The VFX owner now admits 96 meshes and retains the global material bank once. Rendering resolves local IDs through retained mesh tables; legacy embedded materials retain their mapping. Actual APC cockpit residency is 1,348,279 bytes on PC, including 896,000 pixel bytes, within the production 2 MiB cap.

Primary fire uses muzzle_1, 150 armor-piercing damage, speed 250, lifetime 0.5, radius 0.02, spread 1 and cadence 0.08. Eight bounded flights exclude the firing host and retained driver. World, mover, rubble and actor collision select the nearest contact. DEV reserve starts at authored capacity 999; accepted launches alone debit ammo. Fixed tag-forward aim and simple segment tracers are first-pass policies. Texture-alpha collision, exact tracer appearance, muzzle flash and gun audio remain refinements.

PC evidence: 420 frames, one entry/exit, seven launches and wall contacts, reserve 992 and health 5000. Seated cockpit and on-foot endpoint framebuffers inspected. Six focused resource, physics, material, scheduler/flight and rocket-resource tests pass. NXDK build passes. Native run `artifacts/xemu/render-20260918-150747` passes420 frames on64MiB with2904 free pages (11.34MiB), matching vehicle/damage/primary counters. Final on-foot native framebuffer inspected; seated native cockpit and moving tracer visuals remain unverified. The harness closed its emulator and restored the disc.

Not claimed: secondary rockets, articulated turret/from-eye aiming, live NPC damage, standalone shield silhouette collision, APC saves, campaign placement, other vehicle classes or final visual/audio fidelity. Driller checkpoint paths explicitly reject APC state.
