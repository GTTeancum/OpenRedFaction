# Vehicle secondary weapon admission and scheduling

Priority P1. Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Boundary and executed proof

Vehicle alternate firing in4a4e80 dispatches cdecl426ca0(selected weapon owner,0). This routine uses owner+2a8, a distinct secondary weapon ID, rather than current primary+2a4. `artifacts/future-vehicles-re/secondary_admission.py` executes full entry through return or stops at426d73 before muzzle selection.576 cases pass: dying flag, force/bypass argument, timestamp-1/due/future, secondary ID invalid/valid, inventory membership, ammo-1/0/1, scheduling flag and local-owner predicate. Results: secondary-admission.json.

Actual4fa3f0 deadline predicate,4c87f0 weapon flag selector,426f40 failure helper and4fa360 timestamp setters execute. Inventory403250, available ammunition42add0, dying427020 and local-player42a8e0 predicates are supplied; feedback505560 is recorded. No original assets are mutated and no rendered projectile or audio is claimed.

## ABI and fields

426ca0(owner*, byte bypass) rejects dying AL exactly1. If bypass==0, owner+4bc must be due according to4fa3f0. Clock1000 and deadlines-1,1000,1001 respectively reject/admit/reject. Nonzero bypass skips this gate but still enforces valid secondary weapon, inventory membership and positive available ammunition.

Secondary+2a8<0 invokes426f40 and returns. A valid ID absent from inventory returns immediately without backoff. Available ammo<=0 invokes426f40. That actual helper writes now+500 to owner+4bc, then emits505560(2,0,0,1.0) only when supplied42a8e0(owner) is nonzero. All other admission failures leave the timestamp unchanged.

For an admitted non-bypass request,4c87f0(secondary_id,0) checks weapon-record+264 bit08000000. Record base is85cd08 with550 stride. If set, original schedules owner+504=now, copies record+444 to owner+508 and returns without a projectile. The fixture record count7 is copied exactly. Bypass calls skip this scheduling stage and advance directly to426d73; this strongly suggests that argument distinguishes scheduled follow-up from initiating input, but its real scheduler caller remains to prove.

The successful direct path begins incrementing owner+50c for muzzle selection. That stage and all projectile/ammo consumption are outside these576 cases. In particular, a passed admission test does not prove one bullet was emitted.

## Authored relevance and integration

Known authored L1S2 Driller01 UID8122 and L1S3 APC UID9627 are vehicle-use targets. Their exact secondary weapon assignment and authored burst count have not yet been extracted; the synthetic weapon2 and count7 must not become gameplay defaults. Use the original tables and existing src/core/entity_assets.c asset mapping when implementing those classes.

The implementation entry point is the vehicle alternate-fire branch in src/diagnostic/scene.c, feeding shared weapon/entity state in src/core/entity.c. Preserve separate primary/secondary IDs, explicit remaining scheduled shots and independent due timestamps. Reuse the shared game clock; do not fire a normal player-weapon alternate projectile merely because the driver pressed alternate fire.

Open next: find owner+504/+508 scheduler consumer, muzzle arrays and projectile dispatch; recover actual campaign vehicle weapon IDs; verify ammo decrement and timer updates after spawn failure/success. This report is future RE only; main GeoMod implementation remains untouched.
