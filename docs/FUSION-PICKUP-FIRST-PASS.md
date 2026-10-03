# Authored Fusion launcher acquisition

The real installed `shoulder cannon` item now grants `shoulder_cannon` through the shared finite-inventory path. Repeated pickups add shells up to the existing capacity; no separate Fusion ammunition item is fabricated.

Placed items, Give_Item_To_Player metadata and imported ownership now request the Fusion primary definition, first-person view, shell geometry and explosion resources. Resource demand does not grant ownership or force selection. The explicit DEV fusion flag retains its original fixture behavior. The runtime tick is available outside DEV, and scene startup clears stale Fusion input/projectiles. A Fusion-only impact owner does not enable unloaded blood assets.

Focused installed-table pickup checks and PC/NXDK builds pass. The PC fixture `tools/check_fusion_pickup.py` retains original CTF06 geometry and506 props. Two authored grants give one loaded shell and one reserve; eleven normal cycling inputs select slot12, then one shot consumes the loaded shell and launches a real Fusion projectile. The parent inspected the final PC firing image. This establishes scripted acquisition and ordinary selection/firing, not a tested physical world-pickup approach or campaign encounter. Existing Fusion explosion/GeoMod evidence remains separate. Audio was not auditioned.

Native run `artifacts/xemu/render-20260922-171210` reaches123 frames on stock64MiB. Fusion launch/active/error state and player ammo match PC, and the inspected framebuffer shows the launcher firing. The full harness result is FAIL: ROCKET_VISUAL word4 differs (PC353220913, Xbox955152448), while its other seven words agree. Do not count this as full native parity. The invocation accidentally supplied repeated --setup-uid options, leaving only the second legitimate grant active on both platforms, so native reserve is0 rather than the two-grant PC fixture's1. Original disc restoration passes. Projectile-render checksum investigation and a correctly invoked two-grant native replay are deferred; the appropriate argument is --setup-uid 910400 910401. The subsequent unloaded-blood guard was compiled for both targets but was not part of this DEV run. Campaign disk saves, broader campaign encounters, physical pickup traversal and visual refinement remain open.

Settled ownership/ammunition now survives ordinary Xbox save/load and fires afterward; see [Special-weapon saves](SPECIAL-WEAPON-SAVES-FIRST-PASS.md). Ordinary Xbox saves also retain active Fusion flights and reload/cooldown deadlines; pending-flight presentation and broader encounters remain open.


## Ordinary world collection on Xbox (2026-10-03)

The placed-item registry omitted `shoulder cannon` despite existing finite-inventory and resource-demand support. It is now appended at stable class index31; all previous class indices remain unchanged. The extra-weapon grant path resolves the installed weapon name rather than fabricating a legacy slot.

`python tools/xemu_fusion_world_pickup.py` passes on stock64MiB: `artifacts/xemu/fusion-world-pickup-20261003-140358/report.json`,140 frames,5989 free pages (23.39MiB). The fixture copies installed ctf03 item253 into empty CTF06 with only its position changed. Ordinary collection accepts it once; a normal cycle selects Fusion slot12 with one loaded shell and no reserve. There are zero scripted grants, one accepted pickup-audio submission, and the exact notice “Fusion Rocket Launcher and 1 shell picked up”. Fixture/restoration builds pass and original disc flags restore.

This closes the physical world-pickup gap without claiming a campaign placement: installed Fusion placements are multiplayer maps, although the shared single-player collection path is exercised here. This run does not inspect visual/audio output, fire the weapon or test multiplayer respawn. Earlier firing/save evidence remains separate.
