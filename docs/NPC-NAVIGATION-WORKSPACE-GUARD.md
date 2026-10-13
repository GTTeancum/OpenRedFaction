# Shared NPC navigation workspace guard

October 13, 2026: independently accepted source-safety slice; compiled in the 11:00 UTC stock64 batch with a passing minimal startup smoke; see HOURLY-20261013-1100.md.

Ordinary public route requests now honor the same non-reentrant workspace guard already used by tactical and flight searches. Scripted endpoint selection holds that guard across both existing selection passes, graph search and route publication. Its internal graph call uses the already-held lease, avoiding self-denial.

Busy denial returns RF_NOT_FOUND before result, route or diagnostic writes in these wrappers. The movement caller propagates the error before evaluating route miss or direct fallback. Every normal/error return from the inner call releases the wrapper-owned guard. Existing tactical and flight implementations already release their own guard on all post-acquisition exits. No external gameplay/resource callbacks are introduced.

This moves the existing four-byte guard declaration rather than adding actor or global state. It changes no route policy, scheduler budget, save schema, retained route grammar, memory cap or asset requirement. The public function retains its signature. Broader pursuit helpers, execution exclusion and continuation remain external, uncalled staging and are not included by this change.

Review: inspected public and scripted call sites, ordinary graph invocation, tactical and flight cleanup paths, and the scripted caller's error propagation. Source diff checks passed. No functional test was performed; testers own gameplay coverage.
