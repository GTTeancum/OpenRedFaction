# NPC capture failure handling

The two-pass capture used inconsistent membership rules: the first pass skipped every RF_NOT_FOUND, while the second processed every persistence-registered actor and ignored capture failures. A registered actor rejected for an unresolved support owner can return RF_NOT_FOUND before row initialization. With another successful actor, the counted capacity could therefore be smaller than the second pass writes. This is a source-level function-contract defect; ordinary-play reachability has not been established.

The correction omits only absent, unregistered actors, propagates registered-row refusals, and rejects unregistered success. The second pass checks capacity and storage before capturing into the existing local row; it checks capture status and validates the record before copying it. Sorting and count publication occur only after success. Earlier scratch rows can remain written after a later failure; export discards its private scratch on failure.

Independent source review found no concrete problem in this narrow delta. Xbox compilation is pending. No gameplay or save/load test route was run. This change does not establish snapshot consistency, concurrent mutation safety, full restore transaction correctness, or broader pursuit-return readiness. No feature activation, budget or actor-count change is included.
