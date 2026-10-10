# Independent source review: authored waypoint slice

Disposition: approved for parent integration as uncompiled, unrun source. No remaining source blocker identified in the bounded implementation.

Reviewed joint artifact: /workspace/shared/authored-waypoint-joint-final, assembled against clean a8a906fe, whose gameplay baseline is byte-identical to the earlier d1cf6379 source.

Final patch pins:

- Runtime/parser: af48ca5a12c2537677a980208bfbca98a140a75787f81e23b58a718ae1f3f511
- Codec/history: e1c663ccdcdb775f21bddd529bd219f5d23c2e1748b14872db5b83aff61f7187
- Integrated scene.c: 1c6a3559bc48f0c0b81175694d5a02bcacf094e7a28b166ca6e7fd165265ef11

The complete 17-file SHA-256 receipt is /workspace/shared/recovery/authored-waypoint-joint-final-hashes.json. Every non-merged changed file matches its individually reviewed stage. The scene.c merge adds only the reviewed history/capture/handoff hooks to the reviewed runtime scene.

## Confirmed source properties

- Actual bounded L3S1 guard startup paths are substantive navigation gameplay. Constructor path qualification uses the existing named nodes, not an invented event or destination.
- Include order supplies body types and medic qualification before patrol and persistence helpers; patrol helpers precede gameplay/acquisition hooks, and persistence helpers precede RFNC/RFCH consumers.
- Default return preserves shared direction, including reverse Loop, and restarts cursor zero only on a genuine default transition. Exact save/revisit restoration publishes the saved cursor directly.
- Ordinary combat can interrupt actor-default patrol. Explicit event movement, Catatonic, LookAt, ShootAt and animation retain their bounded existing precedence.
- RFNC23/RFCH9 validate stable keys, real event or raw actor provenance, exact source path/hash/cursor, current/shared direction equality and cross-component identity before publication.
- Living section combat/seat state remains a strict limitation; no target-dropping or artificial default conversion is introduced. Every represented current-only combat/seat history entry needs its exact RFNC counterpart.
- Same-level legacy state retains the saved AI/movement. Legacy absent section history cancels only the new constructor addition, including its four AI fields, without clearing independent suppression.
- The final codec delta only adds capture-time raw bounds and owner/follow checks before flag packing. The final runtime delta only separates two return branches with braces/newlines.
- New initializers and two-dimensional character-buffer declaration/definition/caller shapes are consistent by source inspection.

## Capacity and verification limits

The stock profile 0 transport cap is 110524 bytes, unchanged. The 262144-byte override exists only under expanded profile 1 and is not the queued stock cap. The composed load-stage budget remains 2 MiB. The cap/header/whole-load success-boundary files match baseline exactly. Larger valid components may return RF_RANGE; no capacity increase is hidden in this slice.

The replaced global sidecar adds 8192 bytes; each heap history copy gains 73728 bytes. Each RFNC record gains 36 bytes; expected x86 restore-entry growth is 124 bytes including new snapshots and runtime fields. No new automatic full-history stack copy was introduced. Actual compiler layout, stock memory fit, save/load behavior and gameplay remain unverified.

No builds, syntax checks, tests, runtime, campaign routes or active-tree edits were performed by this reviewer. Full review history and corrected cap statement are in authored-waypoint-independent-review.md beside this disposition.
