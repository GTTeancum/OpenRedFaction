# Masako fighter: distinct class and authored Goto

Status: implementation-only isolated slice, 2026-10-08. No compiler, unit test,
gameplay test or XEMU run was performed. The parent owns integration and the
hourly Xbox validation batch. This does not raise the overall approximately88%
working-alpha estimate or establish boss combat completion.

## Original inputs

The installed `tables.vpp/entity.tbl` class `masako_fighter` selects
`Fighter02.v3d`, compiled as `meshes.vpp/Fighter02.v3m`. It independently
specifies movement `fighter` (index9), use `vehicle` (kind1), mass1500, material
metal, maximum speed20, acceleration10, maximum rotation4 and rotation
acceleration4. This permits sharing the existing movement9 dry-flight backend;
it does not permit substituting the regular `Fighter01` class or resource.

Masako's class life is3000, FOV360, armor-piercing damage factor0.5 and energy
factor0.1. It declares `drone explode` with radius12, unlike the regular
fighter. Existing class-specific resource/damage loaders retain these values.
The original L20S2 instance4717 overrides health to2000, armor0, friendliness1,
and starts hidden (`creation_flags=2`) with no seat host. Its primary and
secondary are Fighter Minigun and Fighter Rocket. This slice neither replaces
those authored fields nor fabricates an active player weapon inventory.

The actual Fighter02 model is73711 bytes and has18 tags and two root collision
spheres, without suspension springs:

- csphere02: center(0.0296102781,0.0361495130,-1.4419232607), radius1.6064182520
- csphere01: center(0.0296104923,-0.0795483515,0.9006646872), radius1.7374999523

The resource loader already owns this model in passive resource kind2 and
retains its tags, materials, physics and damage definitions. No driver seat is
required to load that existing passive resource. This implementation borrows
the exact retained kind2 resource, requires `Fighter02.v3m`, derives inertia
from its actual spheres and class mass, and does not allocate a second model.

Original `levels2.vpp/L20S2.rfl` event17973 has parsed type `Goto` (5), even
though its editor display name is `Goto_Player`. Its fixed destination is
(416.0657348633,3.1178033352,-412.7925720215); links are4717 and event18294.
Delay17974 (0.5seconds) links to17973. The class is admitted only for fixed
Goto, with exact original UID and resolved handle membership checks. It is
not silently converted into live-player pursuit or Attack.

## Ownership, movement and persistence

Secondary profile6 maps to passive kind2 and class `masako_fighter`; profile5
remains regular Fighter01. Player boarding, selection, cockpit and switching
continue to admit their existing profiles1..5 only. The shared body initializer
admits the actual movement9 class, while the ground-only wrapper rejects it.

Instance4717 starts as a Hanger Lift001 group child. Original Detach18377
removes live group handles and clears `attached`, but intentionally keeps
`group_owned` as authored provenance for RFVA. Rejecting that historical bit
would permanently block the boss. The new narrow kind2 admission preserves the
provenance and requires both `attached=0` and absence from every current group
controller. A live lift, occupied seat, rider, parked exchange record or active
player vehicle cannot be stolen by the independent solver. Other grouped
vehicle classes retain their previous exclusion.

Once admitted, the existing per-owner rigid state uses the class's movement
values, real two-sphere hull, dry-room/liquid restrictions, static-edge
complement and ordinary owner collision. Freeze/wake, hide, death and removal
retain their existing gates. The table values are grounded; direct point
steering, three-metre arrival tolerance, neutral hover/drag and solid-sphere
self-inertia are existing first-pass policies, not recovered retail boss AI.

RFSV2 stores distinct profile6 in its existing256-byte row, with unchanged
field offsets. Identity, immutable event position, zero navigation scratch,
rigid state, vitals and physics state join the existing RFVA2/RFPV candidates.
Existing RFSV1 and profile2/4/5 values retain their meanings. Older binaries
that do not know profile6 will reject the new boss row.

On fresh boot the original source boss is still lift-attached. Candidate-only
body preparation and identity checks therefore permit that authored kind2
attachment without mutating the live owner. The mandatory RFVA2 join must
contain the same boss with `attached=0`, matching pose/vitals and state. Full
candidate-world dry-hull and collision clearance still run. RFVA publication
rebuilds controller membership before RFSV publication commits motion; no
Detach or Goto event is replayed. Capture has no attachment exception.

## Provenance and remaining scope

- entity.tbl SHA-256:
  `cc512c9213fc87908cd792ed318f14e66f820ff16ff07c5a312c8293ff6827ea`
- Fighter02.v3m SHA-256:
  `d0135be4f24b86e89cf9290f38f8f61c9c3408bab025d77ce65a0fca08da8ec3`
- L20S2 original entity4717 record SHA-256:
  `93cdec78253059378959e3dac7b33c8c530255890ffd3e4273556dbf8d62b8ec`
- Event17973 is at event-section offset647, length78; entity4717 starts at
  RFL offset2837267. Event layout is reconstructed from original462150 and
  type table5a1a3c; model envelopes follow51ce60/53ae5f/5696f0 as recorded in
  the existing source readers.

At the initial dispatch checkpoint the cloud input set contained the VPP assets
but no RF.exe. The exact reference executable was subsequently recovered;
the source-backed generated human phase and death handoff are implemented in
the separate lifecycle slice (VEHICLE-MASAKO-LIFECYCLE.md). This dispatch slice
alone does not implement those phases. Retail AI tactics, cinematic timing,
runtime motion/save continuation and native memory headroom remain open.

No new fixture or exhaustive rejection suite was added. The meaningful next
parent-owned check is the genuine4717 detach/Goto path followed by an ordinary
fresh-load continuation, retaining class6, Fighter02, health2000 and the exact
registered owner without replaying setup.
