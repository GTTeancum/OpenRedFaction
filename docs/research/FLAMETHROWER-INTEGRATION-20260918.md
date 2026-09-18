# Flamethrower first playable primary

The developer weapon cycle now includes Flamethrower after the remote detonator. Primary hold ignites, drains gas continuously, applies fire damage against covered body contacts, loops the authored first-person action and draws a bounded flame jet. Reload uses installed timing and inventory transfer. Campaign selection remains restricted to the existing four slots.

## Implementation and limits

Installed definitions supply primary damage20, pulse interval0.1s, gas magazine100/drain2s, reload2.6s and old-gas discard1.2s. The 0.1s ignition delay and inverse authored view camera are wired at the scene call. A5m expanding contact volume with1m endpoint radius and eight visual samples every three ticks are explicit first-pass port policies, not reconstructed original particle trajectories. Cover uses existing world/rubble queries; each NPC can receive only one damage application per pulse despite multiple body spheres.

The installed flamethrower particle recipe shares Fire01.tga with impact materials. Standalone sprites must use pool0/list2: pool1/list3 does not participate in standalone simulation/drawing. The initial implementation's invisible, accumulating particles were corrected accordingly. The primary loop is admitted by the weapon parser only for continuous primary actions without an explicit fire clip.

Save admission rejects ignition, firing/reload and unsaved fractional fuel state. Completing a reload clears that fuel remainder; general mid-action saves remain unsupported. Alternate canister fire, lingering burns, flame propagation, continuous audio ownership and weapon availability in campaign routes remain open. Sound dispatch is wired; audible output has not been verified.

## Evidence

- Five focused checks pass: flame_stream, scene_flame_gameplay, scene_flame_input, weapon_view_flame, scene_player_weapon_catalog.
- PC and Xbox builds pass after the pending-state save guard and removal of an unreachable duplicate resource assignment in cleanup.
- Native run artifacts/xemu/render-20260918-095132 passes79 comparisons over230 frames on stock64MiB. Flame counters agree:74 active ticks,200 sprites spawned,0 misses,32 peak live. Gas ends at39 loaded/1000 reserve on both platforms. Endpoint available pages2484 =9.703125MiB; this is endpoint headroom, not worst-case campaign memory.
- Native framebuffer inspected: held flamethrower and orange/yellow flame visible against the nearby post; matches PC output. The native run predates only the pending-state save guard and unreachable cleanup-line removal; those received focused checks/build validation, not a repeated native render.
- Separate PC NPC replay artifacts/flame-live/flame.log records8 damage contacts: armor is consumed before health falls to80,60,40. This establishes damage on PC; the native replay above is enemy-free and establishes visual/fuel parity only.
- Original game was not launched. No desktop input or capture app was used. No new GitHub images were uploaded.
