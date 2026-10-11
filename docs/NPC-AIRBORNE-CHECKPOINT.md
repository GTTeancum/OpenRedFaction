# Ordinary airborne guard checkpoint continuation

## Evidence and status

The original L3S1 save attempt at frame120 of the October11 06:00 stock64 batch failed RF_RANGE at NPC capture. The exact rejection was UID929, velocity component1, binary32 bits3227908399 (-3.5933339596). The read-only observer established living guard1 health80, committed fall descriptor3, body flags0x80000079, absent support and real nonzero angular velocity. It did not prove every geometry or resource condition checked by this implementation.

The same hourly batch separately passed the original fresh-start bounded controls check:15 forward-input records and one pistol press produced finite horizontal movement and one shot, loaded16 to15, with no hit. That phase did not save or load. The failed producer generated no promoted save and no consumer run.

This feature preserves an explicitly qualified airborne owner instead of discarding its motion. Source is independently reviewed before integration; compilation and actual new save/load remain pending the next hourly batch. Later capture rows, live qualification, candidate-world placement or resource budgets may still reject. A source-level implementation is not a successful persistence claim.

Raw06 proof: Library libfile_f9e1a96199f08191a22780ef87f3e213 version0, archive SHA256 f20273c24d417e41c6f420224569a1b51d776b95a8414ed41c5f0e52ffc92961. Complete source-design packet: libfile_be928e5175748191894c56698f7bedf7 version0; exact design604303a531d1e0105ed6d577b2026d25fdc21d70ca8f806b10089a90732ea934, independent approval7d692136d2d7e08b1a79607c270a3819662e72f2ec5bbcee3d96d2a5fdba056a.

## Qualified owner

The initial lane is limited to actual ordinary living guard1, authored ult2_guard.vcm and loaded compiled ult2_guard.v3c, exact class flags0x20019/flags2=1/use-kind0/mass100/creation-flags0, immutable class spheres and standing centers, matching class/skeleton/pose bindings and valid full registry generations. It does not hardcode UID929, actor slot, current position, path name or face token413.

Original42268b constructs the0x80000078 physics baseline. Original4281a0 selects fall descriptor3 and sets bit1, yielding0x80000079. The live qualifier admits that exact committed flag state; prepared0x01000000, moving support, sleep, pending or unrelated flags are not silently cleared. Descriptor translation/rotation, material, class speed, mass, bounds and zero authored tensor are checked against retained immutable data. Current, next and published endpoints and reconstructible yaw/eye bases must agree.

Scripted physics, Drone, medic flight, terminal corpses, linked or seated owners, generated bodies and other unsupported owners retain their own existing admission. Presence is an ownership discriminator even when a qualified endpoint happens to have zero velocity. A potential airborne owner that fails complete qualification cannot fall through to the old quiet-state path.

## Retained and canonical state

RFNC26 adds a pointer-free84-byte logical lane:

- Presence, body bits and object bits: three32-bit words.
- Velocity, angular velocity, momentum, force and torque: five exact finite binary32 XYZ vectors.
- Inherited support velocity: a separate finite XYZ vector with the existing absolute-component10000 bound.

There is no invented magnitude cap on the five physical dynamics vectors. The independently saved route fall-speed mirror is not substituted for them. Base support UID/vector remain canonical absence; the new lane owns inherited carry independently after a legitimate support release.

Inactive contact history is canonicalized only under the proven committed ordinary use-kind0 profile: state1240, scalar1441, reference-1, pending word1640 and no support owner. Source collision.c maps word168 to historical face metadata; ordinary query paths produce fresh contact output before feedback reads. Actual413 is not treated as a pending flag or guessed event. Canonical history may differ in diagnostics; bitwise diagnostic parity is not claimed. Active/pending contact remains rejected.

## Codec and compatibility

The new wire tail follows RFNC25's variable flight tail. Absence consumes4 bytes; presence consumes84. RFNC1–25 decode absent. Writer promotion is monotonic, so later flight rows cannot downgrade26. Header layout, catalog/level identity, hash, finite ammo, routes, deadlines, RFCH and shared flight/pain RNG joins remain unchanged. Both pure codec API families can carry the pointer-free lane; only composed fresh scene restoration may publish it.

Full validation rejects conflicting physics/Drone/flight/support/dead/retired/seated owners. A present row requires movement descriptor3 and positive health. All size/span/hash/read/write/version joins are updated together. Maximum structural row is2004 because flight and airborne are mutually exclusive. The actual110524-byte stock file cap and2MiB composed staging cap remain unchanged.

## Candidate placement and publication

A composed fresh load separately qualifies its old constructor owner, builds an independent saved candidate from immutable class data, and installs exact saved dynamics after ordinary pose/bounds/tensor normalization. It checks candidate world volume, movers, props, room and actor/player pairs. The new typed path does not query or invent floor support. Authored-position/rotating/center-shift exceptions are unavailable to this lane; the final constructor-pose pair fallback is disabled for a pair containing it. Independently typed route/pursuit allowances keep their original checks.

The inherited-support store follows the old base support-vector assignment, preventing accidental zeroing. Publication uses canonical inactive contact and absent support/piece/reacquire. The first ordinary resumed step chooses landing or continued fall from a fresh query.

A small loader-local receipt records the existing scene_robot_death_epoch and owner-array identity only when a new lane is present. It is marked immediately after NPC assignment, before later fallible vehicle publication. On publication or final storage-close failure, a two-pass full-identity check precedes restoration of only the new dynamics/contact/support/object-view aliases from existing snapshots. The old compensation owner remains more restricted than the incoming profile, including its old face-zero rule. No full scene, position, route, inventory, animation or resource rollback is claimed beyond established loader behavior. Failed identity preflight makes no compensation writes and preserves the first error.

Standalone and untyped restore paths reject new presence and cannot erase a newly qualified resident owner through legacy absence. Genuine accepted section replacement retains existing constructor policy; this is same-level continuation.

## Resource accounting and limits

Logical record growth is84 bytes. Simultaneous decoded rows and the restore entry's saved/before records add252*N bytes, falling to168*N after decoded rows are freed. Existing full owner/pose snapshots are reused. There is no new per-live-NPC array. Wire growth relative to25 is4*N+80*A for N rows and A airborne rows. Source-accounted stack changes and fixed helper scratch are documented separately; compiler stack usage and stock64 runtime fit have not been measured for this new branch.

A successful build will establish compilation only. A successful ordinary producer, durable exact HDD backup and separate native consumer are required before reporting new save/load behavior. The new L3S1 save cannot recover the missing October9 L11S3 regression image.

## Exact integration receipt

Joint eight-file patch efc72ee037a92d1cc28ad6bcd2632d16e625be6cc6bb10138977bdd2cd4d5668; final independent review351d29a3ef90eb2f3d5558612bbde40a01b815a248ffbf4d18f1fdc2e6563630, retained in NPC-AIRBORNE-JOINT-REVIEW.md. Complete reviewed packet libfile_c9959983af1c819198a8de654a883e58 version0, archive SHA25682b457036d1249cc870db42dfc315441fb3f07fab5bef24dd28586e16a8eb99d. Parent integration follows completed06 and durable proof; the07 build and genuine save/load remain unrun.
