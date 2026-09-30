# Rocket launcher ordinary-level integration

The existing rocket launcher pickup and scripted grant now request sparse first-person resources in ordinary levels. Imported ownership also requests them. Loading resources does not grant ammunition or force selection. The shared flight definition, impact vclip/explosion/Foley and bounded projectile/impact materials are loaded before use. Non-DEV rocket demand does not load unrelated flame or blood textures.

Focused installed-table pickup/import/script demand checks and PC/NXDK builds pass. `tools/check_rocket_pickup.py` keeps original CTF06 spawn, geometry,506 props and pickups with no DEV flag. One real rocket launcher grant and one normal cycle select slot4. A shot consumes one of six loaded rounds, launches a rocket and records one impact. The inspected PC final image shows the launcher and explosion at the far wall. No terrain edits occur because this ordinary-level fixture has no mutable GeoMod owner. Audio is dispatched but not auditioned.

Native run `artifacts/xemu/render-20260922-172811` passes180 frames and all harness comparisons with no DEV flag. Exactly one launch and one impact match PC; the inspected native framebuffer shows the launcher and wall explosion. Stock64MiB retains3341 free pages (13.05MiB), and the original disc is restored. Physical pickup approaches, full campaign mutable terrain, broader encounters and effect polish remain open.

Xbox-only ordinary save continuation now stores active player rockets in the
optional RFWC3 PROJECTILES component, including slot, motion, remaining life,
liquid query flags and draw basis. A rocket-specific player-save admission also
retains the finite weapon cooldown while the launcher remains selected; other
pending trigger phases still block the save. The bounded non-DEV CTF06 fixture
grants the launcher through Give_Item_To_Player, selects it by ordinary input,
fires at frame120 and saves one live rocket at frame122. Stock-64-MiB XEMU
reloaded that payload and reached one impact over the next60frames, with the
saved cooldown counting from74 to15 and3512 free pages (13.72MiB). This is
numeric gameplay evidence; no new visual or audio capture was made. PC remains
a compile-only shared target for current work. Player grenade and other weapon
flight persistence remain open.
