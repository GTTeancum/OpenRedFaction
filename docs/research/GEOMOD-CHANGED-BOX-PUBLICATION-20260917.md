# Original post-edit changed-box producer

Worker state2 at466dcd calls4d0990 on the world owner, iterates its reported
pieces using4d0590, and obtains placement through4d1330. The bounded publication
span466e0a..466ea6 reads piece bounds+48/+54, adds that placement, expands each
minimum by-0.5 and maximum by+0.5, and appends24bytes to648280. Each vector
addition rounds to float before the separate expansion. Count649604 caps writes
at32; this span silently leaves a full list untouched. Stage3 passes this list
to487370 and then resets its count.

`python tools/probe_geomod_changed_box.py` executes that complete publication
span with real40a030,436db0,436d70,409f40 helpers. Piece bounds/placement are
supplied; extraction4d0990/4d0590 and recentering4d1330 are NOT executed here.
27 cases combine three bounds, three placements (including large coordinates),
and counts0/31/32. The complete768-byte array is checked for unintended writes.
All generated raw-word fixtures match rf_geomod_notify_append_fragment_box in
the shared C notification module. The geomod_notify CTest passes, including an
invalid-input output-preservation control.

Original RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Evidence: artifacts/geomod-postedit-re/changed-box-publication.json and
changed-box-build.log. This helper is not yet integrated into live publication;
no emulator acceptance or actor wake behavior is claimed by these tests.

## Integration consequence

The boxes are associated with the worker's processed pieces, not demonstrated
to be the whole room or all modified render-face bounds. Prior advice to use
original/edited post plus neighbor bounds is provisional and must not be treated
as original producer parity. Recover the piece extraction/recentering contract
and connect actual corresponding outputs before enabling this box producer.
The separately verified positive-radius actor notification still applies even
when the piece-box list is empty; do not fabricate one broad box to cover both.
Current terrain component ownership and removals require a focused review next.
