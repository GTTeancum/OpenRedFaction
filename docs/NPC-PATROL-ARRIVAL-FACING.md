# Authored patrol arrival facing and inert-tag admission

Status: integrated and independently source-reviewed, pending the next parent-owned
Xbox compile/package rollup. No builds, tests, runtime, campaign routes, images,
protected-HDD access or pushes were performed. Behavior is unverified by execution.
Baseline: 41004d4756b6ae398f11af3f60594b09aeeb4725.

## Bounded behavior

- Existing ordinary grounded humanoid Loop/Ping Pong defaults may now include
  finite oriented nodes and tag lists that original post-load resolution clears.
  Existing class, default-action, live-body, seat, movement, stand/walk resource,
  active-slot, cache and navigation gates remain binding. One way stays deferred.
- Each raw tag is checked in authored order against the retained decoded event
  owner. The first missing or non-event UID makes the entire effective list empty.
  Empty/cleared lists are admitted; fully resolvable nonempty lists still decline.
  Runtime retirement never changes the original classification. No callback,
  partial-prefix activation, deduplication, UID rewrite or actor-handle lookup is
  introduced; raw tag words/counts remain unchanged for source identity.
- While an existing dwell remains pending, the already advanced cursor and shared
  direction identify the reached node. Its fixed world facing point is authored
  position plus 100 times orientation[0], the first vector in raw disk order.
  The ordinary steer/angular/commit path turns toward it with route translation
  stopped. This does not snap the body to the matrix or own script_look.
- Ground/falling/support processing still occurs first. Existing command,
  combat, seat, freeze, death, retirement and route-loss cancellation is unchanged.
  Wait conversion remains finite ceil(seconds * 60), bounded to 3600 frames.
  Zero wait adds no facing pause. A zero absolute facing point stays absent.

The read-only source inventory identified 13 ordinary actor/default-path candidates
across L1S3, L3S4, L5S2, L5S4 and L6S3 previously rejected at this boundary. These
are source candidates, not a promised live-admission or gameplay-success count.
Six genuinely executable node lists remain a later increment; their Play_Animation,
Look_At and Message composition is deliberately not implemented here.

## Original evidence

Original RF.exe SHA-256:
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
Evidence is retained static disassembly; no original code was executed.

- 0x463d50 reads navigation orientation through 0x52cac0. That helper places the
  first disk vector at destination+0x18, the second at destination, and the third
  at destination+0x0c. Original node+0x60 therefore corresponds to this port's
  orientation[0], not orientation[2]; the port's loader retains raw disk order.
- 0x407a87..0x407ada constructs node position + 100 * node+0x60. The nonoriented
  lane clears the point. 0x40b71b..0x40b75b uses it only while the reached-node
  timer is pending, turns through 0x40cb50, and returns before translation.
- 0x4613dc..0x461451 resolves tags by event UID through 0x4b6820. Any failure
  calls 0x40cb40, clearing the whole count. An actor UID is not an event UID.
  Every all-valid list, including ordered duplicates, remains executable and
  therefore outside this increment.

## Save and resource ownership

No new state, serialized payload, actor field, pool, allocation or cap is added.
The reached node and target derive from the existing next cursor, mode, shared
reverse bit and immutable route source. RFNC32's conditional dwell tail and
RFCH10's conditional history tail retain exactly their existing layouts.

RFNC34 and RFCH11 are semantic reader-capability versions. The source-independent
NPC writer promotes any row with route.patrol&1; history promotes any stored
route with patrol&1, including remote history and a future arrival with no current
dwell. This conservative gate also promotes unchanged previously supported plain
patrols, so those new saves cannot be read by an older build. Saves with no enabled
patrol retain their existing minimum version. No per-row capability marker or
extra source scan is introduced. Older RFNC1-33 and RFCH1-10 remain readable under
the existing source/body/pose/resource gates and absent-route reset policy.

Candidate admission still resolves the actual default route and every source
node; validates reached-node wait against remaining dwell; joins next target,
route section hash and exact owner/movement; and demands the actual selected
stand/walk resources. Raw orientation/tag inputs and existing source fingerprints
are preserved. Staged restore and final-storage-success publication are unchanged.
Loading never dispatches arrival or restarts a default in an absent legacy lane.

Section revisit continues its existing reconstructed-placement policy while
retaining next cursor/direction and remaining dwell. The authored facing point is
fixed in world space, so a carried or reconstructed actor turns toward that point;
this makes no claim of restoring the reached-node body pose. Same-level restore
continues to join the actual saved body/pose. No exact-node proximity restriction
has been added.

## Deferred work

Executable waypoint callbacks, their delayed unfrozen animation composition,
one-way default return/single-node semantics, and wider gameplay coverage remain
separate. Testers own functional coverage; the parent owns hourly compilation and
short startup smoke only. Stock64 fit and runtime behavior are not newly claimed.
