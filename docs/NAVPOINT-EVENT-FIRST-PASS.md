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
checks exact baseline bits and query admission. The full PC scene also accepts
process-local setup events: a 60-frame L6S3 replay of 7124 reports three OFF
writes ending at live radius zero; a 120-frame replay of 7124 then 7123 reports
three OFF and three ON writes ending at the authored radius bits. Both replays
exit successfully. The PC test, PC play build and NXDK build pass. Live actor
travel and Xbox runtime behavior remain open; save coverage is detailed below.

The replay records are 60 or 120 legacy 24-byte zero-input frames in the ignored
`artifacts/navpoint-live/` directory. Run `rf_pc_play.exe --spawn-replay` with
`RF_REPLAY_LEVEL=L6S3.rfl`, `RF_REPLAY_ARCHIVE=levels1.vpp` and
`RF_REPLAY_SETUP_UID=7124` or `7124,7123`; the `SCRIPT_NAVPOINT` line reports
`3 0 0 3 ... 0` for OFF and `3 0 3 3 ... 1058642330` after ON. These checks
establish node mutation in the scene, not observed NPC route changes.

## Ordinary save format

The environment component now writes RFEN2. It appends only nodes whose live
radius bits differ from their authored baseline as `(navigation UID, radius
bits)` rows, eight bytes each. Restore matches the first current node with
that UID, validates the saved radius as zero or baseline, and stages all writes
before publishing any gameplay state. RFEN1 loads with pristine navigation.
This UID-based representation is a port policy; the original save format for
live navigation radius has not been recovered.

The focused environment checkpoint check covers UID reorder, an unchanged
scene before publication, stale-state rejection, RFEN1 decode and restoration
of a zero radius. The event checkpoint codec now accepts type 69. In a full
L18S1 PC scene, Invert 10505 closes node UID 8637 and ordinary save writes
RFEN2 with one row `(8637, 0)`; the saved file is in the ignored
`artifacts/navpoint-live/` directory. A fresh L18S1 load now passes NPC
staging after excluding hidden actors from physical pair clearance. It still
rejects at player placement: the level starts inside the authored
`BustedEscapePod` prop's conservative collision sphere, and the saved player
pose overlaps it. This remains a full-reload blocker; no player clearance rule
has been loosened. PC and NXDK builds pass; native runtime reload remains
unverified.
