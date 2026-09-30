# Grenades in ordinary levels

The installed grenades item now requests sparse first-person and world grenade resources outside DEV. It grants the Grenade weapon and three grenades through existing finite pickup logic; imported ownership and scripted grants also request resources. Weapon world-model loading now follows demand planning so the thrown model is available without unconditional loading.

Grenade simulation runs off-weapon when resources are present; selected grenades bypass magazine/hitscan handling. Scene entry resets throw/flight globals even when the next scene has no grenade demand. Shared impact materials are loaded without unrelated flame/blood assets; detonation uses grenade's authored impact radius and dispatches the shared impact Foley.

Installed-table demand tests and PC/NXDK builds pass. The non-DEV `tools/check_grenade_pickup.py` fixture uses original CTF06 spawn, geometry,506 props and pickups. A real grant at0 and normal cycle30 select slot5; primary input120 has a102-tick release delay and a five-second fuse. The600-frame PC run records one start, one release,11 contacts, one detonation, no remaining projectile and no status/pool/limited errors. Reserve and displayed ammunition fall3 to2. The parent inspected the final PC image: held grenade and residual explosion smoke are visible. Audio was dispatched but not auditioned.

Native run `artifacts/xemu/render-20260922-173326` passes600 frames and all harness comparisons without DEV mode. Throw/release/contact/detonation counters match PC exactly, and the inspected final framebuffer shows the held grenade and residual smoke. Stock64MiB retains3083 free pages (12.04MiB); the original disc is restored. Broader enemy use, liquid behavior, actual pickup traversal and ordinary-level mutable GeoMod remain open; this fixture has no terrain edits.

Xbox-only ordinary RFWC3 saves now retain released player grenades in the
PROJECTILES component, including flight position/velocity, fuse, resting and
bounce-contact state, plus the throw controller's cooldown/edge state. A
controller-only row also preserves cooldown when no grenade flight remains;
mid-windup saves remain rejected because the throw animation is not staged.
The non-DEV CTF06 fixture grants the item through Give_Item_To_Player, selects
it using ordinary input and throws at frame120. Stock-64-MiB XEMU saved one
live grenade at frame225 with178 cooldown ticks, then reloaded it. A60-frame
continuation retained one live flight and119 cooldown ticks. A separate
300-frame reload of that same Xbox payload recorded11 bounces, one detonation,
no live grenade, zero cooldown and3511 free pages (13.71MiB). The guest
checkpoint succeeded in both runs and the staged disc was restored. These
are numeric gameplay checks; no new visual or audio capture was made. PC
remains a compile-only shared target for current work. Liquid response and
other projectile families remain open.
