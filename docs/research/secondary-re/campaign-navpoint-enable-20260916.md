# Enable_Navpoint69: authored UID binding and live navigation admission

Research only; no shared source edits, build, route progression, original game
process, media, emulator or host input. Eight bounded original-code cases pass.
Executable SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

Reproduce from `D:/Programming/GitHub/OpenRedFaction`:

```powershell
python -B tools/future_re/secondary/campaign_navpoint_enable.py
```

Results: `artifacts/secondary-re/campaign/navpoint-enable.json`.
The installed event index contains **41 type69 events in18 levels,54 links**;
the artifact includes all41 authored records. This is a static inventory count,
not41 executed activations. The original nav section is decoded with the retained
`inspect_navigation_records.py` reader; original allocation/loading is supplied.

## Executed contract

Type69 is a derived event. Constructor `0x4bed00` installs vtable `0x589bfc`;
its ON slot is `0x4bcfa0`, OFF slot `0x4bd020`. Calling only the common
`0x4b9070`/`0x4b9f80` switch arms misses this behavior. The factory/constructor
connection is static disassembly; the actual vtable and base activation execute.

At **post-load**, branch `0x4612cb..0x4613c5` recognizes type69 and converts
its links from authored UIDs to navigation array indices. It traverses links
backward and navigation nodes forward, comparing raw32-bit UID at node+0x6c.
The first matching node wins. Missing UIDs are removed with actual `0x4ce390`;
surviving link order and duplicate links remain. This does **not** apply generic
object flags, sentinel exclusions or object/key type resolution. A colliding
object UID is irrelevant. This conversion is not repeated during activation.

Both handlers traverse the converted indices in forward order, skipping signed
negative values and values >=navigation count (`[0x6460e8]+0x300` array).
ON copies the raw32-bit baseline radius at node+0x18 to live radius+0x1c;
OFF writes positive zero to+0x1c. No enabled flag is toggled. Full0x100-byte
synthetic node footprints prove no other node field changes in these cases.
ON can restore negative-zero bits, rather than inventing a positive/default radius.
Base activation `0x4b8b70` suppresses action when event+0x2b0 mask0x1 is set.
Type69 does not generically propagate its nav links as object events.

The L6S3 composition uses actual Invert ON, common dispatch, `0x4b6640`,
`0x4b6800`, base activation and the derived OFF handler. Only object lookup
`0x40a0e0` is supplied for event7123; it returns event+4 and the real type6
check executes. Navigation handlers require no supplied service calls.

| Authored action | Resolved targets in write order | Live radius result |
|---|---|---|
| Shot1 Delay6852 -> Invert7124 ON -> event7123 OFF | UID3778/index21, UID6624/index58, UID6621/index56 | 0,0,0 |
| Shot14 Delay6912 -> event7123 ON | Same three indices | 2,2,0.6000000238418579 |

All three nodes have height2 and word+0x40=0. Their serialized first byte is0;
it is discarded by the original loader and does not make their initial radius0.
The retained `tools/verify_navigation_loader.py` proves `0x463d50/0x40e9e0`
initialization copies the serialized radius to both+0x18 and+0x1c. It was read
and reused here, not rerun or counted among the new eight cases.

## Gameplay consumer, not presentation

Actual `0x40c570` compares requested radius against live node+0x1c, requested
height against+0x20, then applies the mode-low-byte1/word+0x40 gate. New
composition checks radius0.5,height1,mode0: all three authored nodes admit it
before OFF, reject after OFF, admit after ON. **Radius0 still passes OFF** with
the same height/mode; this is radius admission, not a universal boolean disable.
The previous full consumer implementation already preserves the original
floating comparison behavior; no new NaN or pathfinding grid is claimed here.

Current `rf_entity_navigation_candidate_allowed` (`src/core/entity.c:1612`)
implements this predicate. Existing selector sites at1865 and2035 read the
same mutable candidate.radius; `rf_level_navigation_workspace` borrows those
candidates. Thus subsequent qualifying navigation selection sees the change.
Neither fully executed69 handler calls a route invalidator, changes adjacency,
or modifies retained-route state. This report does **not** establish immediate
replanning, refusal of every existing path, or the behavior of an actor already
following a retained route. Do not add unconditional route cancellation on this
evidence. No presentation consumer was required to establish the gameplay effect.

## Smallest current C integration

The required owners already exist: `rf_level_navigation_node.uid`,
`candidate.retained_018` (baseline bits) and `candidate.radius` (live value),
`include/rf/level.h:72`; `navigation_scan`, `src/core/level.c:515`, initializes
both radii at532–533. No new per-node runtime field or pathfinding owner is needed.
The scene's `campaign_navigation` owns these nodes; its workspace and selection
references borrow them rather than independent radius copies.

1. Add a once-only type69 navigation binding stage after navigation is loaded
   and before startup actions. `rf_runtime_events_resolve`, event.c:1097,
   currently sends every event link through object/key resolution; scene.c:4608
   invokes it. Add a dedicated nav resolver at that join, using authored UID
   order and first matching navigation UID, independently of generic link.kind.
   **Do not let kind0 generic links suppress the nav callback, and do not resolve
   these UIDs again on each ON/OFF.** Scene teardown/reload requires fresh binding.
2. A small proposed representation can reuse the existing12-byte
   `rf_level_link_target` storage: introduce named link kind3=NAVPOINT with
   value/index=nav index; missing nav UIDs become kind0 permanently for this
   binding. Other events retain current kinds0/1/2. Keeping inert holes instead
   of physically compacting the port array produces the same survivor write
   order for69; this is a proposed representation difference, not original
   array behavior. Never pass kind3 to `startup_target`'s object registry path.
3. Add type69 ON/OFF handling in `startup_event_action`, event.c:543, before
   its unsupported fallback. Use a nav callback/context in
   `rf_runtime_triggers` (event.h:309), installed with scene callbacks around
   scene.c:13301 and owning `campaign_navigation`. Process only NAVPOINT links
   in surviving authored order. Existing `rf_event_activate` owns disabled and
   delay policy. Keep69 nonpropagating.
4. Add callback-gated69 to `rf_runtime_events_tick`'s supported pending list,
   event.c:1001–1018. L6S3/7123 has delay0; generic nonzero authored69 delays
   must reach this same action instead of remaining permanently unsupported.
   This package does not repeat the existing delayed-event timer proof.

Uncompiled scene callback sketch, with binding/index validation owned by caller:

```c
static int campaign_navpoint(void *context, uint32_t index, uint32_t on)
{
    rf_level_owned_navigation *nav = context;
    rf_entity_navigation_candidate *candidate;
    if (index >= nav->count) return RF_NOT_FOUND;
    candidate = &nav->nodes[index].candidate;
    if (on) memcpy(&candidate->radius, &candidate->retained_018, 4);
    else candidate->radius = 0.0f;
    return RF_OK;
}
```

This proposal adds zero node bytes, zero link-allocation bytes and no navigation
registration. A callback plus context adds8 bytes to a32-bit trigger owner;
update existing `sizeof` budget accounting. No cap or shared implementation was
changed. Source line numbers describe the research snapshot and can move.

## Persistence and acceptance

Original save serialization of this live radius is **not recovered or executed**
here. The proven mutable state is node+0x1c; baseline+0x18 remains unchanged.
Proposed port persistence stores a bounded per-level `(nav UID,live radius bits)`
override (8 payload bytes per entry, separate container/allocator accounting),
not a transient node index, registry handle, object flag or actor mission.flags.
Rebuild nodes/bindings from authored data, apply overrides by first UID match
before gameplay resumes, and keep the authored baseline available for later ON.
Missing saved UIDs can be ignored with a diagnostic. That restore/missing-UID
policy is a proposed port decision, not measured original-save behavior.

| Acceptance | Evidence/required result |
|---|---|
| L6S3 point1 chain | Actual Invert ON ->69 OFF writes indices21,58,56 in order; all positive-radius0.5 queries reject. |
| L6S3 point14 | Direct ON restores exact2/2/0.600000024 baseline bits and query acceptance. |
| Missing and duplicate UIDs | Post-load input999,20,10,999,10 becomes1,0,0; missing removed, first matching node chosen, repeated writes preserved. |
| Invalid runtime index | Negative and >=count skip; valid index still mutates for ON and OFF. |
| Disabled event | Base disabled bit prevents all node writes. |
| Raw baseline | ON restores negative zero; zero-radius query still passes with OFF/zero radius. |
| Delayed integration | Required future port test: supported pending69 fires through same nav callback at existing timer deadline. Not newly executed. |
| Restore integration | Required future port test: UID overrides survive owner reorder; subsequent ON restores authored baseline. Proposed/static. |

The original execution stops at the post-load branch boundary and supplies owner
arrays, node records and one object lookup. No full level loading, navigation
search, actor travel, cutscene playback or save/load executes. This closes the
small69 dependency identified by `campaign-l6s3-dependencies-20260916.md`;
cutscene integration still has the independently documented motion residency
blocker and55/83/63 adapters.
