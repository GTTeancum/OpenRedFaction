# Enable_Navpoint first pass

The campaign event `Enable_Navpoint` (type 69) now binds its authored link UIDs to
the level's navigation nodes after both owners load. An OFF action writes zero to
each linked node's live radius; ON restores the node's original radius bits. The
shared event dispatcher handles immediate and pending actions, and the scene's
navigation consumers read these mutable nodes. Missing UIDs remain inert. This
does not force an NPC already following a route to replan.

The behavior comes from original binary ON handler `0x4bcfa0`, OFF handler
`0x4bd020`, post-load binder `0x4612cb..0x4613c5`, and navigation predicate
`0x40c570`. The bounded reconstruction and its limits are recorded in
`docs/research/secondary-re/campaign-navpoint-enable-20260916.md`. The port keeps
inert holes where the original binder removes missing links; surviving writes
retain their authored order.

The installed L6S3 test fires Invert 7124 into Enable_Navpoint 7123 OFF, checks
nodes 21, 58 and 56 reject a positive-radius query, then fires 7123 ON and
checks exact baseline bits and query admission. The PC test, PC play build and
NXDK build pass. Live actor travel, Xbox runtime behavior and save persistence
of changed live radii remain open.
