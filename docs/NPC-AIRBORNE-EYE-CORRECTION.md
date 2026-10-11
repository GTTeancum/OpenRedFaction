# RFNC26 ordinary eye reconstruction correction and failure localization

## Actual 07 result: what is established

The original L3S1 neutral180/save120 producer at game-source commit cd1ab654c81dabc5e5dee3434763fed4c7092901 rejected UID929 with tuple [929,461161902,0,1325400064,4294967294,120]. FNV-1a of `ordinary_airborne` is461161902. This is the aggregate return at baseline scene_npc_checkpoint_capture.inc357, after the new potential-owner branch has been entered and its qualifier returns RF_FORMAT(-2). It is before the old velocity rejection. The 128-word velocity bank is zero because every generic rejection clears it and only the later velocity branch fills it.

1325400064 is0x4f000000, binary32 representation of2147483648. The generic rejection converts the body uint32 flags through double to binary32; the actual low flag bits are not retained. This terminal tuple therefore does not establish exact current flags0x80000079. The earlier actual06 observer did retain that exact value, but cannot substitute for actual07 evidence.

No nested identity, geometry, basis, support or finite-vector predicate is identified by the07 tuple. No successful NPC capture, codec payload, promoted save, consumer load or new persistence behavior is established. The zero-byte stage2 refusal remains authoritative.

## Proven intrinsic source mismatch, independently of that missing observation

Original RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836 distinguishes two owners:

- The ordinary branch at49d184..49d1f6 updates body angles, builds body4a0d70, adds body864 to eye87c via40a030 and builds eye4a0d70.40a030 stores each of the three sum components to binary32.4a0d70 uses pitch and yaw; its finite roll input is ignored.
- The separate physics branch at49fe7a..49feb1 copies the body matrix, applies pitch from894+87c and roll from89c+884 about local axes, and does not apply eye yaw.

The reconstructed rf_ordinary_motion_commit reproduces the first branch with combined[i]=(float)((double)body_angles[i]+eye.angles_87c[i]) and the private look_orientation_calculate. The RFNC26 endpoint proof and existing generic candidate builder instead used rf_eye_physics_orientation, the second convention. Nonzero eye yaw is lost there, and pitch construction is not generally equivalent. This is an intrinsic wrong-owner reconstruction, whether or not actual07 failed at that predicate.

## Complete narrow correction

The new pure rf_ordinary_eye_orientation exposes the same private look_orientation_calculate and binary32 addition convention used by the existing ordinary commit. It validates finite input and preserves output on error. It does not step eye state, add offsets, clamp, wrap, query, publish, or mutate any owner. Crucially, it does not call public rf_look_orientation on the combined angles: that API's body-domain ±pi/2 pitch and ±2pi yaw restriction is narrower than the ordinary committed sum.

The new-airborne endpoint checks its live eye basis against this ordinary construction, with the existing finite1e-6 basis tolerance. The independently built saved candidate uses the same pure helper only when rows[i].airborne.present. Saved eye yaw is already in RFNC's common eye_angles lane; no codec or version change is needed.

The existing ordinary commit is byte-for-byte unchanged. Legacy, Drone, flight, script-physics and seated candidate branches retain their current policy. All other new-airborne identity, class, geometry, descriptor, body/object flag, contact, support, dynamics, candidate placement and publication checks remain. The constructor still has the same strict identity, pitch/roll, yaw/body/model basis and endpoint gates; its eye must now agree with the exact ordinary reconstruction that will be published for this lane. No authored pose is coerced to pass.

## Failure-only discriminator

The staged16-word bank records only the actual first qualifier failure. Predicate expansion preserves logical conditions, short-circuit guard ordering and returned status; it introduces no new owner reads before the guards protecting them. Source-line sites bind to the exact staged source hash. Matrix/vector parts identify the first failing component. Integer/body flag values are uint32; observed float operands are copied directly from source storage via memcpy rather than converted through double or passed by float value.

The receipt is local until the unchanged generic `ordinary_airborne` rejection is produced. Only then does capture publish the exact six-word tuple join, source owner index, source-line site, part, payload kind, actual/reference operands and exact body flags. The format word is assigned last. Generic rejection and top-level capture clear the bank, preventing stale evidence from being interpreted as a current qualifier result. The bank is never an admission input.

Restore and lifetime checks pass NULL for the optional failure receipt, preserving their existing error channels. No restore failure is reclassified as a successful capture. The native harness changes only by retaining the16 existing guest words at the new exported symbol; input, frame120 schedule, acceptance, HDD and retry policy are unchanged.

## Bounds and verification status

Six changed source files are identified in manifest.json and correction.patch. Additional diagnostic global storage is64 bytes. The local receipt is20 bytes; capture adds one descriptor pointer. Endpoint removes the72-byte temporary rf_eye_angle_state; the pure helper needs12 bytes of combined angles plus a loop index. Float comparison references are bounded scalar temporaries. There is no new heap allocation, persistent NPC field, per-row array or serialized data. Code size and compiled stack/memory fit remain unmeasured.

No repository integration, syntax check, build, test, game-code import, emulator, gameplay fixture, image, process lock, HDD modification or retry was performed. The only execution used for this packet was file/source staging and read-only inspection/disassembly of retained source and original binary. Independent exact review and parent integration remain required. A later successful save/load can establish its own outcome; it cannot retroactively prove the precise first predicate that rejected actual07.

## Parent integration

Exact six-file correction patch9663a47596694d70d43bd7000ff457679c07be82338a40232c2d4ecf31a04783 was integrated after terminal07 proof and final independent review383d018e758a2ad93bbf6898ddde94a3d214601ab5025550a35b6aec9b74744c. Durable packet libfile_87e290e11360819181e3f2580ebd6bb3 version0, archive SHA25673ae68da3cdcf64516bfd7556ebdc49ec6559343335d4d3452fa3495fde4651e. New source and read-only observation await the08 hourly build; no new runtime outcome is claimed.
