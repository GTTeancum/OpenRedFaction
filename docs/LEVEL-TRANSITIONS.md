# Campaign level transition integration

Load_Level (event type22) previously reached unsupported_actions. It now invokes
an optional borrowed backend on its on action; off is a no-op. Delayed events are
eligible for the existing timer dispatcher when the backend is attached. Without
one, the existing unsupported-action/pending accounting remains available.

The scene attaches rf_scene_level_transition as an owned deferred request. It
copies destination/entrance strings, raw words/flags and event/source/actor IDs.
The request survives event/archive destruction; IDs are provenance, not promises
that the objects remain registered afterward. No archive I/O or scene destruction
occurs inside event dispatch. The first request remains pending until its owner
resets it, so later events cannot overwrite the selected destination.

The destination validator accepts a bounded simple level basename, adds .rfl
when absent, normalizes the extension, and rejects directory paths or malformed
names. This is a port loading policy. Entrance text and unverified words/flags
are retained without inventing semantics. Platform consumption is not connected:
this change queues exits but does not automatically enter a second level.

Tests cover first-request ownership, malformed-name rollback, missing backend,
250ms delayed activation, and14 authored exits across L1S1/L1S2/L1S3. Each target
exists in levels1.vpp, including both forward/backward and current-level targets.
All requests remain intact after closing their source resources. Both builds and
33 CTests pass. Evidence: artifacts/level-transition-authored.log and
artifacts/level-transition-tests.log. This is shared C dispatch verification;
no original full Load_Level oracle or native transition is claimed.

Next steps: establish current-level request behavior (several authored pairs name
the current level), preserve the appropriate player placement/vitals/inventory,
release the current level completely, resolve the destination archive, and load
through the shared PC/Xbox lifecycle. Prevent overlapping level residency on64MiB.

Shared destination resolution now exists as rf_level_campaign_open. It searches
levels1/2/3.vpp in order with one candidate archive open at a time, reads only
level metadata, and transfers archive ownership to the caller on success. The
returned level borrows the caller's archive, not a temporary stack handle.
Failure preserves both outputs; reopening an already-owned archive is rejected.
Missing/corrupt required archives fail immediately; an absent destination returns
NOT_FOUND after the search. This is an explicit first-pass archive policy, not
original engine search-order reconstruction; multiplayer archives are excluded.

The authored transition test resolves all 68 installed campaign level entries
across the three archives and checks missing targets/directories, output rollback,
open-handle protection, case-insensitive names and trailing directory separators.
Both builds and all 33 CTests pass. Evidence: artifacts/campaign-resolver-tests.log
and artifacts/campaign-resolver-destinations.log. Platform loading-loop consumption,
player-state transfer and native cross-level playback remain unimplemented.
