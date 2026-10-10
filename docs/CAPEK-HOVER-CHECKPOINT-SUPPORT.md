# Capek hover checkpoint support

Source-only correction proposed 2026-10-10 after the actual noon saved-HDD
regression. No new build, test, syntax check, fixture or runtime was performed.

## Actual failure and regression boundary

The immutable Oct9 10:00 L11S3 RFNC14 save previously passed a fresh Xbox load
at633a8a4e (Oct9 16:00). The noon consumer59fc9e0b built successfully but failed
at frame1 with RF_NOT_FOUND (-3), before any RFNC14 physics assignment.
Original inputs, saved HDD and evidence restoration checks all passed.

Raw evidence is `/workspace/shared/rf-frozen-npc-20261010-1200/verification.json`
and `load/result.json`; the successful comparison is
`/workspace/shared/rf-frozen-npc-20261009-1600/verification.json`.

The exact FNV1a phase hashes decode as follows:

-933488787: ordinary loader `world`.
-1739935381: world restore `npc_stage`, with93472 staged bytes,
  19 NPC /19 mover /118 prop rows, last UID4938 /saved row0.
-893222784: NPC restore `placement`, row0 /UID4938 /RF_NOT_FOUND,
  after all19 resident rows were admitted.

The finer placement array is `[5,1,0,2,4294967293,0,0,0,0]`:
phase5 floor admission, matched1, mover solid index0, material2, status-3.
Static volume fit and all candidate mover/prop obstacle checks already passed.
This does not establish the contact fraction, normal, mover UID or actual
penetration; that information is not recorded by this array.

UID4938 is the original selected-Cane Capek at
(52.03443908691406,-62.60279846191406,-150.49700927734375), class0, living,
movement present/slot12/normal1, support UID0, no RFNC scripted-physics body.
The original placed owner is documented in CAPEK-CANE-FIRST-PASS.md and the
source read of levels2.vpp/L11S3.rfl. The failing saved row is byte-identical
to the successful16:00 consumer's row. This is not Eos/miner freeze rejection.

Commit5d55baed correctly resolves exact unbroken Capek's effective base speed
as8.0 rather than authored0.3, including candidate-world placement. At the
unchanged dt1/60, rf_physics_ground_prepare therefore changes its downward
endpoint offset from0.055 to0.183333 units (both probes start0.05 above the
body origin). The longer optional floor sweep reaches mover0. Because the
saved row has no mover support, the existing support guard rejects that hit.
The placement adapter is byte-identical between633a8a4e and59fc9e0b; its
speed-dependent floor acquisition was already inappropriate for hover and
became observable after the correct speed increase. Keep the speed fix.

The noon failure precedes motion selection and all new residual fire/reload
clip ownership checks. Those guards neither caused this recorded failure nor
have they been proved passable by this failed load.

## Original support ownership

Read-only RF.exe disassembly confirms487f82 calls the falling predicate42a020.
If not falling,487f94..487f98 requires movement descriptor index1 before it
can call4a0840 support at487fc1. Nonfalling hover12 does not query a floor.
The existing rf_entity_support_route and NPC-MARKERS.md576 describe the same
boundary. rf_entity_falling recognizes modes3/8 and unsupported use-kind1;
the new candidate qualification explicitly excludes those cases.

Original constructor422aed..422af0 retains authored class speed, while
422b23 and422b26 install8.0 into class/base and live speed. This correction
must not revert8.0, shorten a sweep artificially, fabricate support, change
saved bytes or relax any actual collision volume check.

## Bounded implementation

Only the ordinary NPC world adapter may set the new default-zero
`npc_hover_no_support` proof. It requires:

- The existing exact immutable Capek + authored hover12 + Nano identity.
- Living, unretired, non-death, unbroken saved state.
- Existing ordinary_no_contact proof: neither falling/moving-support body
  bits nor a support handle, and explicit saved support UID0.
- Saved movement descriptor12; RFNC1..12 uses the same enabled constructor12
  fallback already selected by NPC restore, never the resident live mode.
- A nonfalling use-kind under the existing falling predicate.

All other owners, broken Capek/run/fall/death states, active support, seats,
player/vehicle adapters and disabled legacy fallback0 retain existing behavior.
The context rejects the new proof if allow_no_contact is absent, a support UID
is supplied, or a player static-support provider is present.

With this proof, scene_checkpoint_world_place still runs unchanged static
world sphere/basis/center classification and unchanged candidate mover/prop
obstacle clearance (including existing authored-overlap policy). It then
returns only the explicit absent-support sentinel handle0/material-1 through
the existing final room lookup. It does not run a downward support-acquisition
sweep for a movement form that cannot own the result. Later NPC pair fit,
support admission, composed world dependencies and validate-before-assign
remain unchanged. No actor state or live resources change during preparation.

No RFNC format, saved payload, source asset, weapon state, speed policy or
animation ownership mask changes. One uint32 private world-context word is
accounted for by existing sizeof-based staging budgets.

## Verification boundary and next observation

Source review only. Compilation and runtime remain unverified. The parent
owns the next scheduled Xbox build and any reuse of the exact existing saved
HDD under snapshot mode. A future successful load must still establish all
19 rows, actual RFNC14 affected-owner assignments, the32-frame neutral result,
and every original-input/restoration guard. If a later stage fails, retain its
new evidence and diagnose it separately; this patch does not preapprove it.

Independent review approved this narrow source correction. Existing rows without physics continuation still use the resident body/support admission rule; loading a legacy save over a later falling or supported live owner can remain conservatively rejected. This unchanged limitation is not broadened here.
