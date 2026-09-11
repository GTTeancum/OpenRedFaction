# Named NPC animation marker consumption

Original `RF.exe` SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

`51c420` reads the dominant active-slot index at model instance +1d48, resolves
its motion ID through the active slots at +12d4 and descriptor array +f5c, then
searches two names at motion descriptor +40 and +54. Each name is 16 bytes with
a following tick value. Empty names are skipped and comparison is case-sensitive.
The first exact match returns false if its event byte is clear. If set, it clears
only that byte (+1d44 or +1d45) and returns true in AL. A duplicate second name
is not consulted after a first match, even if only the second event is pending.
No dominant slot or absent name returns false without clearing any event.
Upper EAX bits are not a boolean contract; the reconstruction returns a normalized
result via `fired`.

`rf_motion_consume_marker` in the shared motion core implements this behavior
using resource-indexed `rf_motion_marker_names`. The port validates bounded slot,
resource and string views. Failure preserves playback state and output. Metadata
must describe the resolved dominant resource, not merely active-slot order.
This function does not play a sound or choose whether an actor should poll it.
The caller's footstep dispatch/gates and catalog name retention remain open.

`python tools/verify_motion_marker_consume.py` executes the complete original
routine unchanged and compares PC and NXDK machine code. It covers 512 cases
with 47 true events: selected slots/resource IDs, exact-case and mismatched names,
empty/missing/duplicate names and all two-bit event combinations. Four invalid
port input cases preserve state/output. Result: PASS in
`artifacts/motion-marker-consume.json`. Both builds and all nine CTest checks pass.

## Entity footstep request routing

The only direct call to51c420 is501d3d inside the kind-two model wrapper501d30;
503420 forwards to that wrapper. Its two entity call sites,42f999 and42fa4e,
belong to42f940. `rf_entity_plan_footsteps` reconstructs that routine through
the48a930 sound-dispatch boundary:

- Entity+200 must equal -1; otherwise markers remain untouched.
- Flag8 at+7c, a nonzero+1430 player record and view mode zero (player+c4,
  then view+8) route to42fb20, bypassing model markers. This alternate route
  is reported, not executed by the planner.
- Poll left then right. Resolve the class group at+294/+178 indexed by entity
  +1380, falling back to index zero for a negative group. The table's declared
  footstep material groups support naming this selector `surface`; runtime
  assignment/ground-query integration remains to recover.
- If both selected/default groups are negative, return immediately. A consumed
  left marker can therefore leave the right marker pending.
-434d40 returns the sound-group count;434df0 returns its sample-list pointer.
  Left selects the first half, right the second half, using signed division by
  two. For valid nonnegative counts, odd trailing samples are outside both halves.
- Requests copy entity+3c and subtract+180 from Y. Their final scalar arguments
  are 1.0 followed by global62f980. Their deeper audio meaning is not assumed.

The port returns sample-array indices instead of process pointers, and validates
resource bounds. It does not choose the random sample or run the audio backend.
48a930 performs selection and 2D/3D dispatch; those paths, sound-group loading,
marker-name retention and the calling entity-update schedule remain open.

`python tools/verify_entity_footsteps.py` executes the complete original42f940,
including its real marker wrappers, group getters and vector-copy calls. Only
42fb20 and48a930 are intercepted to observe the two dispatch boundaries. PC and
NXDK machine code match640 cases, including85 alternate routes,213 sound
requests and4 left-failure/right-pending cases. Report:
`artifacts/entity-footsteps.json`. Both builds and all nine CTest checks pass.


## Foley group parsing evidence

`foley.tbl` owns the named groups referenced by entity footstep declarations.
Original `434880` opens that table; `434960` writes 44-byte group records at
`6300f8` (32-byte name, material, signed sample count, sample-array pointer).
The decompile shows a default count of one when `$Sounds:` is absent and a
loop over exactly the declared count. Missing material leaves the existing
record field untouched; a reconstructed owner must account for initialization.

`python tools/verify_foley_parser.py` executes original `511fc0` and `5125c0`
without hooks, including their whitespace/comment and CRT callees. Eight
primitive cases verify that forward search is case-sensitive and can skip
arbitrary intervening rows, while optional-token consumption is case-insensitive
and only accepts the current token after whitespace/comments. A supplied stop
token prevents searching past it; `434960` supplies no stop token for `$Name:`.
The search advances CR-based line accounting across skipped rows.

Two checks using the installed Foley table confirm that searches starting at
the fifth sample of Default Footstep and Solid Footstep skip both surplus rows
and land on the next group. Each declares four samples but lists six. Do not
silently add those two samples when reconstructing the group owner: this would
change the left/right halves consumed by `42f940`.

Evidence: `artifacts/foley-parser.json`, original executable SHA256 as recorded
above. This verifier covers parser primitives and real table tails, not the full
`434960` loader, allocation or registration. The candidate per-sample reader
`434620` reads a quoted filename, returns -1 for an empty name, otherwise reads
two floats and calls `5054b0` with an additional 1.0 scalar. Registration argument
semantics and runtime ownership still require direct verification before hookup.
