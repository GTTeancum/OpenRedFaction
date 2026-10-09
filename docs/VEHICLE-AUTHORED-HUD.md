# Authored vehicle HUDs

Source-written 2026-10-09 for the parent-coordinated 01:00 UTC Xbox batch.
No build, test, emulator run, screenshot, capture or image generation was
performed for this implementation. Source inspection is not native validation.

## What the original game actually provides

The submarine has its own cockpit HUD. It is authored geometry with dynamically
selected texture frames, rather than another flat health/ammo overlay. The
installed `sub.vfx` contains three hull-health digits, three torpedo digits and
two bank lamps. RF.exe also supports submarine gauge materials, but those two
material names do not occur in the installed cockpit, so no gauges are invented.

The five playable vehicle profiles use these distinct presentations:

- Driller: original `health_driller.tga` plus `health_vehicleframe.tga`, raw hull
  health at `hud.tbl` rows45–47, and its existing authored cockpit animation.
- Jeep driver/gunner: original `health_jeep.tga` and health frame at rows42–44;
  the gunner also receives the ordinary clipless ammunition panel/reticle.
  The driver has neither ammunition overlay nor a firing reticle.
- APC: cockpit hull/primary/secondary gauges plus three hull, four minigun and
  three rocket digits; the standard weapon reticle remains screen-centered.
- Submarine: six live cockpit digits and authored idle bank-lamp frames. No
  generic screen-space reticle, human-health silhouette or “GUN” counter.
- Fighter: three hull, four minigun and three rocket cockpit digits, with idle
  bank-lamp frames. No generic screen-space reticle or duplicate flat counters.

Masako's special boss chassis is not a separately boardable player HUD profile.
The mounted Jeep gun's separate first-person VCM is a distinct rendering slice;
this HUD change does not fabricate it or replace its placement model.

## Reconstructed source evidence

RF1.20 NA original executable, inspected read-only:

- `0x439bf2..0x439c2e` loads the Jeep/Driller/frame art directly, without `_0`.
- `0x439d80..0x439f57` branches on occupied-host kind, draws the two original
  ground silhouettes with white alpha120, and prints the host's raw health.
  The authored positions are `hud.tbl` rows42–47. Other cockpit vehicles return
  without drawing the ordinary human health/suit display.
- `0x43a371..0x43a39a` permits the ordinary in-vehicle reticle only for the Jeep
  gunner or APC; `0x43a3e2` excludes the Jeep driver. `0x42ac80` and `0x42acd0`
  distinguish first/second Jeep occupant; `0x40cac0` checks class flag0x200.
  Installed `weapons.tbl` assigns `reticle.tga` to APC Minigun, APC Rocket
  and Jeep Gun. No weapon entry references `reticle_apc.tga`, so the previous
  guessed APC-specific reticle is corrected. `ammo.tbl` maps `turret_ammo`
  to the standard `bullet_icon.tga`.
- `0x4a79f0` dispatches cockpit material replacement to APC/Fighter `0x4a7a80`
  or submarine `0x4a8080`; `0x4a7d80`, `0x4a7e00`, `0x4a82a0` load the real banks.
- APC banks: `apc_digits.vbm`, `apc_armor.vbm`, `apc_primary.vbm`,
  `apc_secondary.vbm`. Fighter banks: `Fighter01_digits.vbm`, `fighter01_lb.vbm`,
  `fighter01_rb.vbm`. Sub banks: `sub_digits.vbm`, `sub_lb.vbm`, `sub_rb.vbm`;
  `sub_armor.vbm`/`sub_primary.vbm` are only loaded if matching surfaces exist.
- `0x4a7eb0` selects decimal digit frames. Sub primary names1–3 represent
  hundreds/tens/ones; APC/Fighter primary names1–4 represent thousands through
  ones. Original names deliberately spell `cocpit`, and must remain exact.
- `0x4a7ef0`, `0x4a7f60`, `0x4a7ff0` select bounded gauge frames using
  ceil(current/maximum × (frame_count−1)). Their `0x57385b` CRT helper installs
  x87 upward-rounding control0x1b3f; digit extraction's `0x573e83` uses
  downward-rounding0x173f. The animated bitmap descriptor precedes its frame
  records (`0x50f961`, `0x50fa7f`), so original base+1 is retained image index0.

## Ownership and implementation

`scene_vehicle_cockpit_hud.inc` identifies each authored material once, replaces
its loader-owned bitmap name with the appropriate original VBM and lets the
existing texture owner deduplicate all digit surfaces to one bank. Original
placeholder pixels are released before loading replacements. Startup peak is
the larger of the two sequential owners, never their sum. Existing cockpit,
profile-pack, aggregate scene and stock-64-MiB limits remain unchanged.

The existing scene material merge and profile handoff carry these banks without
new allocations or ownership paths. Once per cockpit presentation, a read-only
snapshot selects frames from actual hull health and the live weapon reserves.
The authored instance loadout mask maps absent/unsupported weapons to zero
rather than exposing dormant reserve values. The existing VFX geometry, UVs,
placement, brightness/opacity tracks and driver animation clock are retained.
The scene frame renderer changes only the material index of each bound face.

Read-only asset-header arithmetic gives these decoded-RGBA pixel totals, before
metadata/page rounding; these are estimates from original assets, not executed
loader measurements:

- Sub: 496,640 → 545,792 bytes, 20 → 26 retained frames
- Fighter: 864,256 → 901,120 bytes, 16 → 18 retained frames
- APC: 896,000 → 1,192,960 bytes, 29 → 63 retained frames

The three ground-health sprites join the existing 512×1024 HUD atlas with no
atlas-dimension or 2304-KiB budget increase. Their source dimensions are
112×52 (Jeep), 176×49 (Driller) and64×24 (frame).

## Native verification telemetry

`rf_scene_vehicle_cockpit_hud[16]`:

0 profile (2 APC,4 Sub,5 Fighter;0 non-state cockpit);1 bound material count;
2 presentations;3 bound faces actually emitted by the latest cockpit draw;
4 raw hull HP (nonnegative truncated integer, bounded99999);5 primary rounds;
6 secondary rounds;7–9 hull/primary/secondary gauge indices (zero-based);
10–12 hull/primary/secondary decimal digits packed into nibbles;13 status;
14 retained cockpit texture-bank count;15 present channel bitmask.

Bitmask:1 hull gauge,2 primary gauge,3 secondary gauge;4–6 hull hundreds/tens/
ones;7–10 primary thousands/hundreds/tens/ones;11–13 secondary hundreds/tens/
ones;14/15 left/right bank. Installed Sub uses4–6,8–10,14–15; Fighter4–15;
APC1–13. There are no leading blanks. Three hull digits reproduce the original
modulo1000 behavior. A small ammo debit need not cross a gauge bucket; compare
live reserve and packed digits instead. Telemetry is presentation-local and
persists after exit; use the ordinary occupied/frame diagnostics as well.

`rf_hud_vehicle_diagnostic[8]`:0 art-HUD vehicle presentations;1 current profile;
2 ground-health sprite40 Jeep/41 Driller (0 absent);3 raw hull HP;4/5 masked
primary/secondary reserve (-1 means unavailable);6 reticle22 APC/Jeep gunner
(0 absent);7 draw status. Slots1–7 clear on an ordinary on-foot art-HUD frame.

## Deliberate boundaries

Banks display the original idle frame. Alternating tube/bank firing lamps depend
on the original firing-side owner, which the current one-muzzle adapters do not
represent; no fake clock or alternating state is introduced. Exact cockpit
lighting/translucency and cosmetic parity remain deferred. HUD fallback stays
available if atlas admission fails. Native memory, submitted-face coverage,
board/fire/exit behavior and visual appearance remain unverified until the
parent batch; the lack of captures precludes a visual-parity claim.

### 01:00 batch compile correction

The first parent build stopped before runtime on four declaration-order errors:
the art-HUD include preceded the complete vehicle runtime and Jeep-seat types.
The two raw runtime reads now use a forward-declared, read-only presentation
snapshot defined beside the existing vehicle HUD accessor. This preserves all
HUD behavior and ownership. The correction is source-written and awaits the
parent's retry within the same consolidated batch; no helper build/test ran.

### APC-only section resource demand correction

The parent's same-batch L15S1 APC retry reached startup stage50 and returned
RF_RANGE with7028 free pages both before and after the failing call. This was
not an allocation failure: the shared explosion-image owner was demanded by
`scene_vehicle_ai_ready`, which includes APC, but its authored definition was
loaded only for Sub/Fighter or independent handheld-explosive demand. An APC-only
section left `campaign_rocket_impact.resolved` zero, failing the material loader's
input gate before allocation. The same vehicle demand now loads that definition
for APC as well. The existing128-KiB impact-image cap and every scene/stock64-MiB
bound remain unchanged. Parent runtime retry remains required.
