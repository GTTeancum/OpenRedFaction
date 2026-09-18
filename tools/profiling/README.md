# Optional collision work profiling

`fragment-inner-work.patch` temporarily adds serialized diagnostic work counters and exact PC/Xbox counter checks. It was captured againstc788c9c2 and its scene/harness context was updated for the edge and moving-contact audits, with translating-mover work included in the mover counter group. Do not keep it enabled in normal gameplay: native mean active debris time increased from67.5ms to85.6ms in the measured run. Operation counts remain useful; timing with counters is not a release-performance result.

From the repository root, ensure the affected files have no unrelated changes, then run `git apply --check tools/profiling/fragment-inner-work.patch` and `git apply tools/profiling/fragment-inner-work.patch`. Rebuild the PC target before launching any replay; Windows cannot replace a running executable. Run the existing XEMU render harness with the desired DEV input. The report records `fragment_stage_work` and compares32 words to PC.

After all owned processes finish, use `git apply -R --check tools/profiling/fragment-inner-work.patch` and `git apply -R tools/profiling/fragment-inner-work.patch`, then rebuild both PC and Xbox. This patch does not alter inputs, collision decisions or save format. It can need rebasing after later collision changes; always check before applying/reversing.

Evidence: `docs/research/GEOMOD-FRAGMENT-INNER-WORK-20260918.md`.
