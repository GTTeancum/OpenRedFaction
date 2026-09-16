# Vehicle weapon firing ownership and jeep gate

Priority P1. Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. This is a bounded prefix, not a complete firing implementation or rendered vehicle proof.

## Executed evidence

`tools/future_re/vehicle_fire_owner.py` executes unchanged4a5029..4a50a8 within player firing4a4e80. Entry fixture supplies EBX=player actor and a valid emulator stack; original instructions select ESI=firing owner. It stops at either admitted4a50a8 or reject4a58b8, without executing the surrounding function prologue/epilogue. Entity lookup, death predicate427020 and gunner predicate42acd0 are supplied; actual486c90 use-kind and40a2f0 jeep predicates execute.

All256 cases pass: host present/absent; host use kind0/1/4/7; selected owner weapon-1/3; actor/host dying; owner jeep flag; gunner predicate. Results fire-owner.json verify owner and admission. Earlier weapon animation/permission gates and all subsequent fire cadence, ammo, muzzle pose and projectile creation are explicitly outside this test.

## State rule and ABI

4a4e80 is cdecl(player*, alternate flag, edge/trigger flag); the larger routine's raw export is available in artifacts/analysis/rf_b8fb9ab4c9bf/4a4e80.c.txt. This prefix reads actor+200 and resolves its host through426fc0. Owner remains actor unless host exists and actual use-kind is4 (turret) or1 (vehicle), in which case owner becomes host. It then rejects selected owner+2a4 weapon<0, actor death, or owner death. Host death has no influence when host is not a selected vehicle/turret owner.

If selected owner's class+724 has jeep bit400000,42acd0(player actor) must be nonzero; otherwise firing rejects. A jeep driver therefore supplies movement but cannot fire the jeep's gun through this path; the jeep gunner can. Both seat roles still choose the same host as weapon owner, so firing ownership cannot simply reuse movement ownership: the preceding4a6060 proof keeps gunner movement/aim record on the passenger while this path chooses host weapon+2a4.

## Campaign and implementation

L1S2 Driller01 UID8122 and L1S3 APC UID9627 are use-kind1 targets; selecting their host weapon rather than Parker's held weapon is necessary for eventual vehicle combat. Their actual weapon selection/cadence has not been verified by these synthetic fixtures. L12S1 jeep entry event9694 supplies an authored jeep control example from the campaign worker, not proof of a player-operated gunner encounter.

Integrate a distinct firing-owner resolver into src/diagnostic/scene.c combat dispatch, backed by the existing shared entity use-kind/occupant predicates in src/core/entity.c. Preserve actor identity for death checks, input/aim and attribution while using host weapon/ammo for selected vehicle/turret hosts. Validate host handles and weapon IDs explicitly before mutation. These data and state boundaries can be implemented in shared C/C++ without original layout copies.

Remaining: recover owner-relative muzzle transform, primary/alternate ammo/cadence and continuous-fire release; vehicle projectile eligibility/damage; actual authored weapon values; driver/gunner camera constraints. The high-impact distinction established here is gunner actor input versus host gun ownership.
