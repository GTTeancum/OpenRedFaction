# Independent Fighter live-player pursuit

Status: implemented and verified in an isolated cloud copy with two bounded stock64MiB Xbox boots, exact ordinary-save/fresh-load continuation and ordinary OFF. Publication/integration into the frozen parent checkout is still pending. No percentage increase is claimed.

## Authored gap and implementation

Original `levels2.vpp/L20S2.rfl` contains genuine type6 chains:

- UnHide18157 → Goto_Player18156 → Fighter18155
- UnHide18169 → Goto_Player18170 → Fighter18167
- UnHide18200 → Goto_Player18201 → Fighter18199
- UnHide18274 → Goto_Player18275 → Fighter18273

The existing dispatcher already forwards type6 to the movement callback, but the independent vehicle runtime and checkpoint admission accepted only type5 Goto. The independent Fighter now accepts type6 after validating the original parsed event identity and its original/resolved owner links. The parsed type and runtime type must agree. The editor display name is irrelevant. APC remains fixed-Goto-only.

The runtime caches point/player order kind in RAM. A player order samples the current registered live player's body position when generating each flight command, leaving the authored point unchanged. Inside the existing3 m arrival tolerance, it releases controls but retains the active order; later player motion resumes pursuit. A fixed Goto retains its existing terminal arrival behavior. Missing/dead/removed players produce neutral controls without deleting the order. A render-hidden seated player remains an eligible target, provided its body and registry identities are valid.

The owner OFF, freeze, visibility, death, retirement, empty-seat and roof-rider gates remain in their original paths. OFF disables further steering commands and allows normal drag/coasting; it is not an immediate physics freeze. No selected-player vehicle behavior, route planning, avoidance, autonomous firing or rider transport is added.

## Persistence

RFSV1/2 envelope sizes, row offsets, reserved words and writer selection are unchanged. The authored point remains bit-exact in the original target fields. No moving player sample or cached kind is serialized. Restore reconstructs kind only after validating the event UID, authoritative type, original/resolved owner links and exact authored point. Normal validation then checks that the cached kind agrees. Publication does not execute setup or replay the order.

The existing successful checkpoint audit additionally compares reconstructed kind, alongside exact row re-encoding. The new pursuit observer tests staged kind reconstruction and rejects a deliberately mismatched cached kind on a private candidate.

## Prepared checks and honest coverage

`tests/scene_secondary_vehicle_order_tests.c` calls the same pure event/steering helpers as the runtime. Its13 groups cover misleading display names, Fighter-only type6 admission, original/resolved identity, duplicate links, fixed destinations, positive/negative moving-player response, persistent arrival and subsequent resumed control, fixed arrival completion, missing targets and atomic nonfinite rejection. All13 groups pass in the focused host unit harness and natively on Xbox; the complete Xbox build also passes with the project’s `-Werror` settings.

`python3 tests/fighter_player_pursuit_harness_tests.py` passes5 synthetic validator/replay checks. They accept a consistent fabricated telemetry trace and reject stale target samples, incorrect actual steering and an extra presentation-frame step, and verify bounded movement-only guest input. These test the harness behavior; the fabricated trace is not Xbox evidence.

`tools/xemu_fighter_player_pursuit.py` requires explicit `--prepare-only PATH` or `--run-root PATH`. The preparation-only source check passed. It retains Fighter18155 and the two original chain records byte-for-byte, plus27 unchanged sections including geometry, navigation and original player start. Other actors, controllers, triggers, clutter and items are isolated. Three ordinary events provide Delay startup and a delayed Invert OFF. Process-contained RFI6 inputs move the player; the observer never moves actors or changes live orders.

The accepted source/fresh pair is120 frames each on stock64 MiB. Acceptance requires actual Fighter movement and yaw response, sampled targets equal to the prior committed live player position, independently computed commands differing from stale-player/fixed-point alternatives, exact complete saved rigid/order/owner/player and event/timer restoration without setup replay, and ordinary OFF halting further pursuit sampling while physics keeps stepping.

The opt-in native observer also runs the13 pure groups,10 copied-state/staged-save checks, and the existing checkpoint rejection/atomicity audit. Copied-state checks cover retired/frozen/hidden/dead/seated/attached pause predicates and inactive neutral controls. They do not establish natural arrival, live riders, missing/dead-player integration, full route completion or all four encounters. Both boots pass those bounded requirements.

Source RFL SHA-256: `9fdc28ed4181580c3de40c5998724d70246aa924897fda38c1ec089eb5a92fd2`.

Prepared fixture RFL SHA-256: `1e1e59b474e608e6e7969cb57c2c9b5c59181ca79d13ea8df1a67fda32c7332e`.

Prepared fixture VPP SHA-256: `d7468829f44a58117c8c488851303dcf5bac3bd7e6de04e8a258cfec4a55951e`.

Original UnHide18157 SHA-256: `3a1629b7ff30789a5117131c3db785a1dc0929f979d6a4309f6fc951ac82ea52`.

Original Goto_Player18156 SHA-256: `10a0ae74b35620a18a51d438f8886646268c7846208516207bb77222781a42f8`.

## Bounded Xbox results

The source boot is preserved under `artifacts/xemu/fighter-pursuit-20261008-0824`; its raw report retains the initial FAIL. The harness originally compared every pose flag against damage flags, but the actual pose correctly retains capability bit`0x04000000`. The corrected assertion compares only removal/hidden bits`2|0x4000`, preserving ownership, visibility and vitals checks. An earlier isolated staging attempt also remains preserved under`fighter-pursuit-20261008-0823`: it exposed a missing disc-directory creation step, then successfully compiled the full Xbox target during restoration. No guest ran in that attempt.

The source report was revalidated read-only, and only the fresh boot was run under `artifacts/xemu/fighter-pursuit-20261008-0828`. That report is PASS and links the immutable earlier source report by hash. Source and loaded PE/map hashes match. A post-build guard also ensures future fresh launches cannot silently use a different rebuilt engine.

- Source:120 frames,6450 free pages, living player,118 independent flight commands/steps. Player displacement is3.998762 m;97 command frames differ from a frozen initial player target, and86 show physical yaw response in the commanded direction. The Fighter moves1.679726 m over frames20–40.
- Source reaches a static-contact stop late: pose/basis remain identical from frames100–119 while active pursuit and solver stepping continue. No avoidance is claimed.
- Ordinary save is4184 bytes, including an unchanged-layout RFSV2 row and RFVA2 with no selected host. Native checks pass13 steering groups,10 copied-state/staged-save cases and18 checkpoint rejection/atomicity cases.
- Fresh:120 frames,6291 free pages, living player, no setup replay and no new save. Frame0 exactly restores all64 serialized row words,34 rigid-state words, player position, full owner/player handles, order kind2 and source event/timer state.
- After player movement, fresh flight resumes from the earlier contact:1.807981 m over frames20–40,56 moving-player-response frames,14 physical yaw-response frames and3.508951 m player displacement. All61 active commands use the current player sample rather than the authored point.
- The ordinary delayed Invert delivers one OFF. Command sampling stops at61; owner stepping continues to118 with ordinary coasting. Both boots record119 controller advance/commit pairs and no duplicates. The final presentation frame advances no state.
- All39 tracked disc-input paths restore exactly after both completed boots. No images, host input, campaign progression or GitHub writes were performed.

Exact accepted payload and binaries:

- Save RFWC SHA-256: `fc5853ae463867e865155996aca42f0d01e970792015b6b43862a9ddb02f5282`
- Common PE,3698176 bytes: `1a1696eda49fa95856bd0c13ef1a2c77709260942aa66f63957db8f303d57e46`
- Common map: `d48a119133afd15f4f2078ed243df946e3fab6f65e8989558f6c9173091dc5b1`
- Source XBE,3710976 bytes: `d2f16bad22b76dd8c205a6f0ee8457e3307d83de83117163f6a3de34feedd3f2`
- Fresh XBE,3710976 bytes: `22a2c48ee05f69954cf30022446b52c0d0784fcbd8f26b4ac3796b24500319a6`

Independent source and raw-evidence review found no remaining blocking issue. Natural arrival, live riders, missing/dead-player integration, obstacle avoidance, all four encounters and autonomous weapons remain outside this claim. RFSV1 layout/reader compatibility is source-preserved; no additional legacy guest boot was run for this slice.
