# Original player boarding prerequisites

Priority: P1, ahead of vehicle implementation. This is original-code research, not a playable vehicle result.

Original RF.exe SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Executed boundary

`4a1970(player_owner*, actor*)` is cdecl. For actor host handle `actor+200 == -1`, `4897d0(actor,&tag)` supplies the use target; its handle at `+2c` is resolved through `426fc0`. The actual original `486c90` classification dispatches use kinds 1 (vehicle) and 4 (turret) through the same prerequisites. The proof stops at `4a1d52`, before weapon-delay, water-only and attachment effects.

`tools/future_re/vehicle_boarding_gates.py` executes 810 cases: both use kinds; multiplayer byte 0, 1 and 2; all 128 boolean combinations of seven supplied query results plus each query independently returning 2. It verifies the complete called-query sequence and whether execution reaches the admitted boundary. The target/handle lookup and query services are supplied, so this does not prove their internals or spatial use-target selection. `boarding-gates.json` records each case.

The multiplayer byte at `64ecb9` rejects exactly 1 here; exit rejects any nonzero value. Normal booleans are 0/1; the difference is recorded for exact evidence rather than proposed game behavior.

| Order | Original query | Call arguments | Rejection |
| --- | --- | --- | --- |
| 1 | `42a130` | host, selected tag | AL nonzero |
| 2 | `4a7690` | host | AL nonzero |
| 3 | `425250` | actor | AL exactly 1 |
| 4 | `4a9dc0` | player owner | AL nonzero |
| 5 | `4a7420` | player owner | AL nonzero |
| 6 | `40a0d0` | thiscall ECX = player+`b8` | AL nonzero |
| 7 | `4adb60` | player owner | AL exactly 1 |

Rejection falls back to `4c0100(actor,1)`; the fixture returns true there. No later fallback or audio implementation is exercised.

## Additional static semantics

Disassembly identifies `4a7690` as null-safe host flag `+814 & 800`. `425250` checks player weapon action 8 when actor+1430 exists, then actor+810 bit100. `4a9dc0` applies only to the local player (`7c75d4`), checking player+f80 != -1 or action6/action7. `4adb60` similarly checks action10/action11. The reference WeaponAction enum names these reload, draw/holster and custom-start/custom-leave; action-index calls are executable evidence, names are corroborating reference labels. `4a7420` resolves player+14 and checks timestamp words actor+4c0 and +4c4 via40a0d0. This report does not assign an unverified duration or trigger to those words.

`42a130` has a distinct seat policy: if tag != -1 and host predicate40a2f0 holds, it searches host+8cc and retains the last matching tag, resolving that seat's occupant with426fc0. A stale occupant handle therefore appears unoccupied to this query, although427240 rejects any non--1 handle. If no seat-search branch, it returns host+810 bit10000. This is static evidence and an important reason to validate the final attachment return in the reconstructed implementation.

## Authored campaign relevance

Existing installed-data inventory supplies L1S2 Driller01 UID8122 (use kind1, movement5, class flags561161) and L1S3 APC UID9627 (use kind1, movement5, class flags557569). Both need boarding eligibility before core controls can be exercised. These examples establish authored vehicles, not successful original or reconstructed boarding in those levels.

## Implementation boundary and remaining work

Retain the shared class/use-kind parsing in `src/core/entity.c` and `src/core/entity_assets.c`; live DEV vehicle interaction belongs at the use-action boundary in `src/diagnostic/scene.c`. Seat ownership, reload/weapon-transition exclusion and event pulses should compose as explicit state, rather than treating427240 as the complete vehicle controller. No source changes are part of this report.

Still open: use-target distance/tag selection; exact timestamp ownership; water-only admission; post-attachment controls/camera/weapon routing. Original4a1e41 ignores427240's AL return after emitting vehicle entry pulse bit400; copying this partial-failure behavior is not required for product-first implementation. The campaign worker owns event77/78 pulse consumption and Never_Leave80 policy.
