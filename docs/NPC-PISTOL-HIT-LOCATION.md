# NPC pistol body hit-location

## Status and exact review

Source reconstruction and outside-repository implementation are complete; immutable v2 passed independent static review against baseline `24c453fc7cf2d7da650d4031408e6423a3efba9f`. Integration and scheduled Xbox validation remain parent-owned. No compile, syntax test, runtime, fixture or save-compatibility result is claimed.

- Final three-file patch SHA256: `7c8400119eb7ed040eed3138f2d32ebdb25b8305e2178519d9ff2d9e6394dc80`.
- Independent exact-code review SHA256: `9072934614eaec714cf8a7927688bdfbfc4fc165de36839ecc6db041faa5dc56`.
- Source contract SHA256: `62b7ffb9adc6ae025e3ccbe9245e8163a1c592daf1a16f893870641195840239`; independent design review SHA256: `84039e4bff68d19377734b7f13cf2b83155661b5c5d6bcd7e9628a8611b6b515`.
- Original RF.exe SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

The patch adds `scene_ai_pistol_location.inc`, wires the accepted ordinary shot in `scene.c`, and extracts only the existing ordered scalar loop from `scene_npc_hit_location.inc`. Earlier PLAYER arithmetic, triangle query and reaction handling are unchanged.

## Original source qualification

An ordinary NPC pistol projectile can receive a nonplayer humanoid's posed-CSPH damage factor. Its contact is physical sphere response, not the PLAYER triangle response. `48c2a0..48c2bf` adds model-response flags only for a player-bit8 source; NPC-owned kind2 therefore uses `49a420`. Factory `4c7934` supplies flags0x80000870; `4c7a0f/4c7a11` creates kind2. The actual pistol's empty projectile model selects the one zero-centered fallback sphere at `49efa4..49efea`/`49f106..49f13e`.

Known L3S1 guard1 UID109 provides the bounded ordinary primary12mm profile: radius0.02, speed275, damage40, AI scale1, nonpiercing bullet kind1 and undeviating flag0x20. These bind actual class/weapon/capability metadata, never a UID. Already-retained guard1, guard2 and medic1 target metadata provide three physical/CSPH rows and SP factors respectively0.5/1/5,0.5/1/3 and0.5/1/5. Guard109 affiliation0 to unlinked medic783 affiliation1 establishes conditional reachability; no actual shot is claimed.

`48c26d` rejects self and live linked-Use1; `48c7f0` supplies forward, affiliation/target560 and undeviating-plane gates. `4c5a52..4c5a68` separately rejects a target linked to the projectile source. The player-controlled-target gate `48aaf0` limits miss helper4c8840; ordinary unlinked NPC targets bypass it.

`49a420` retains strict AABB overlap, radius sum, ordered strict-earlier time and initial-overlap rules. `49a85e..49a91f` produces the sphere surface point, including the projectile-radius normal offset. Then `42ce00`/`42cf5b` selects the strict minimum posed-center distance/resolved radius in authored order; the physical winning sphere is irrelevant. AI-to-player remains unscaled for location. `4c5d52..4c5d6a` applies this scalar to fresh AI damage before `4c9b20` and generic4892c0. Nano's earlier4c5a7f..4c5b5d consumer and held shields receive raw damage.

## Implemented scope and policies

Only registered kind0, nonplayer, unlinked guard1 sources carrying the exact existing ordinary finite primary pistol enter the new shot lane. Accepted source, descriptor geometry, base damage and world epoch are captured before presentation. A qualified accepted single-fire FIFO request is consumed before callbacks and presentation uses its captured copy, preventing replay or popping a replacement request.

Qualified target bodies require existing guard1/guard2/medic1 metadata, current evaluated pose and full identity. Private one-versus-three-sphere response uses the existing finite instantaneous segment with target current transform frozen as both current and next. This is the explicit port scheduling policy, not original projectile flight. No scratch physics contact/flags are published and no projectile owner is allocated. Independent body/shield and cross-actor strict-nearer ordering still bounds clutter, fragments, vehicles and world cover. Only a surviving qualified body receives fresh base times scalar once.

Zero/zero affiliation retains prior unscaled proxy policy: neither `combat_target` nor intended handle represents original target560 generally. Other classes, unsupported weapons, generic links, player-controlled targets, body0x400 and active Nano retain explicit prior policy; known-class live linked-Use1 remains an original body rejection. A supported miss/rejection/error cannot fall back to proxy damage. Guard-to-guard parity is not established.

Existing `scene_robot_death_epoch` fences world/timeline replacement; full registry/slot/handle and target class/pose/geometry checks remain separate. Changed contacts cancel after callbacks. Damage/death and later caller bookkeeping re-resolve the original handles; a replaced world exits the old tick. Intended AI targeting/pursuit is unchanged, although corrected physical contact may legitimately change the intercepted actor.

## Remaining validation and one adjacent gap

Conservative Xbox32 source accounting is2596 added declared scratch bytes, below the3072-byte design cap, with one shot, one winner, one candidate and two response views. Compiler frames/spills, existing pose/cover/callback stack, actual stack fit and stock64-MiB behavior remain unmeasured until the scheduled parent batch. There is no new persistent/save state.

One adjacent source-only lead is location-directed pain-action publication in this same admitted pistol consumer. Original `4c5d6e..4c5d89` gates the reaction block; `4c5e93..4c5fbd` uses the selected CSPH index and published actor frame, with index0/1 writing828 values24–27. The v2 AI lane retains index but deliberately does not publish that action. Existing PLAYER reaction code cannot be adopted blindly: the next bounded qualification should establish this ordinary NPC pistol branch, published3c/48 ownership and ordering before generic damage, without new owners, death-hint reconstruction or implementation until separately approved.
