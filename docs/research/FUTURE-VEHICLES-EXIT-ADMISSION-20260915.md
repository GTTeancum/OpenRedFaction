# Future vehicles: exit candidate admission

Priority: P1 before playable campaign vehicles. Current standalone scene lacks board/exit ownership and vehicle control integration. This is ahead-of-implementation research, not a claim that vehicles now work.

## Existing coverage and selected gap

Existing shared code already covers class/use-kind/movement tables, occupant predicates, ordinary physics dispatch, APC/Driller contact selection and several collision effect services. Those are documented in ENTITY.md, DEATH-LIFECYCLE.md and LEVEL-ENTITIES.md and are not reimplemented here. Missing boundary selected: safely leaving a controlled host, then committing occupant/control release. Alpine's reference header names4279d0 entity_detach_from_host but declares void; original instructions return a meaningful boolean in AL.

## Executed original admission prefix

`python tools/future_re/vehicle_exit_candidates.py` executes original4279d0 through either its early return or commit boundary427c1e. RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.48 cases cover each first-free candidate0..4, all blocked, object versus world blockers, upright/inverted hosts and Driller flag. Real vector routines and486c90 classification execute. Entity lookup426fc0 and collision services49b900/499ed0 are supplied; feedback505560 is recorded; mutation/commit is stopped before427c1e. Results: exit-candidates.json. Source disassembly:4279d0.disasm.txt.

ABI: cdecl(actor pointer), AL=false on null actor, unresolved actor+200 host, or exhausted candidate search; AL=true only after the later commit tail. Successful prefix tests reach commit rather than observing its final AL. The actor and host buffers are unchanged throughout all48 tested prefixes.

For class/use-kind1 host (actual486c90 reads type0 class+1b4), candidate search uses host physics position+e4 and physics orientation axes+fc/+108/+114:

1. Host up axis, choosing its sign so upY is nonnegative, scaled by host+180.
2. Negative right axis, scaled by host+180.
3. Positive right axis, scaled by host+180.
4. Positive forward axis, scaled by host+180.
5. Negative forward axis, scaled by host+180.

If host upY>0 and class flags+724 include0x1000, the first three candidates use half host+180; forward candidates still use full host+180. This is the Driller flag from the existing class parser. The up sign/half-scale gate treats equality separately; this fixture executes upY+1/-1, while the zero boundary is currently static instruction evidence.

Every candidate is added to host physics position. Candidate Y is clamped upward to at least occupant physics Y at+e8 (static inspected427b85..427b9c; fixtures use occupantY9 below candidateY10/14). Then49b900(actor,actor+e4,candidate,query,host) runs first. If it reports blocked, world collision is skipped. Otherwise499ed0(actor+e4,candidate,actor+88,query) runs. Query constructor40ea60 executes, and query+18 is set float1 before collision. Both services must report clear to reach commit. All five blocked causes505560(2,0,0,1.0f) then AL=false. This call is recorded as blocked feedback, not asserted to be a particular audible asset.

Non-kind1 hosts bypass vehicle candidate search and proceed with the actor's existing published position+3c. Their commit behavior remains the next task.

## Authored campaign examples

Existing archive-backed seed probe, rerun without rebuilding:

- levels1.vpp/L1S2.rfl: UID8122, classDriller01, use-kind1, movement index5, class flags561161. Exercises the half-radius branch when upright.
- levels1.vpp/L1S3.rfl: classAPC, including UID9627, use-kind1, movement index5, class flags557569. This is an authored placement, not evidence that a player can enter it in the reconstruction.

## Implementation handoff

Add an explicit vehicle-use/exit service around the current actor use/combat input path in src/diagnostic/scene.c; do not hide it inside shared collision or merely clear host200. A shared caller-owned exit-candidate helper can consume host physics pose, radius, class flags and occupant pose, with existing actor collision services supplying clearance. Return a rejected exit without changing seat/host/control state when all candidates fail.

The actual49b900/499ed0 flags and query semantics need matching to the shared collision facade; this probe establishes their argument/order boundary, not full collision equivalence. Next is427380 seat removal and4279d0 commit tail (host flags, weapon/control cleanup, placement/orientation, AI target retargeting). Camera and input possession must be recovered independently before live integration. Vehicle weapon/physics/damage coverage remains incomplete.
