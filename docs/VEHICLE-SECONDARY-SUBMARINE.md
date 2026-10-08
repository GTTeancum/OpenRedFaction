# Independent submarine pursuit

The parent's12:00 UTC combined c29d6f NXDK build passes. Runtime verification
remains pending. No new fixture, test suite, campaign replay or
emulator session was created for this slice.

## Authored need

Original `levels1.vpp/L5S4.rfl` keeps selected submarine3955 and a second
submarine2381. UID2381 is authored hidden, unseated and hostility0, with
health700, primary Torpedo and secondary none. Its pose is
(-69.734222,-18.186829,-30.630920), with no moving-group reference.
Original Goto_Player3966 links2381 with5seconds delay and flags[1,0];
Goto_Player4455 links the same owner with2seconds delay and flags[0,0].
The existing event dispatcher forwarded these commands, but secondary motion
admitted only APC and Fighter owners, so the independent submarine had no
movement consumer. Neither event is rewritten or automatically activated.

The shared submarine implementation already derives its actual Sub_Mini01
hull, class7 movement metadata and table values: mass2500, speed6,
acceleration8, rotation4 and rotation acceleration2. See
`SUBMARINE-FIRST-PASS.md` for the existing selected-host implementation and
its earlier bounded evidence. This change reuses those providers; it does not
reinterpret a Fighter body as a submarine or copy the selected host's physics.

## Runtime

The secondary pool now maps resource kind0 to profile4 and class`sub`.
Allocation/admission retains the original kind11 registration, damage owner,
source UID, visibility and physics flags. Genuine type5/type6 records must
match both authored UID links and resolved full handles. Empty/unattached
ownership and rider restrictions remain. Already parked switch-bank owners
cannot acquire another independent physics authority.

Submarines use their own wet-hull motion adapter and table-derived parameters.
Every original hull sphere must fit entirely below a water surface, and the
swept path may not cross a liquid boundary. Ordinary static, mover, rubble,
actor and other-vehicle collision stays enabled. The existing static-edge
complement now serves both independent free-flight classes. It does not
replace water admission or manufacture a geometry-free path.

Goto retains its authored point. Goto_Player samples the registered live
player each step, including a render-hidden seated player. Missing/dead/removed
players produce neutral controls while retaining the order. Arrival uses the
selected-submarine's existing1.5m horizontal/vertical tolerance and bounded
throttle/rise/yaw gains. Fixed Goto completes; player pursuit stays armed and
can resume after the player moves. Hidden/frozen/retired/dead/occupied/ridden
owners retain their existing pause gates. OFF releases commands and allows
normal drag; it does not become an implicit freeze.

The selected3955 remains separately controlled. The source passive2381 pose,
accepted velocity and rigid state are committed through the same independent
owner transaction used by the previously implemented APC/Fighter pool.

## Saves

RFSV2's existing profile word now admits actual profile4. Header and256-byte
owner row layouts remain unchanged; RFSV1 remains APC-only. The class/resource
mapping, original order identity, cached order kind, exact immutable authored
point, rigid pose/velocity/momentum and passive damage/visibility projection
are validated before assignment. A submarine has no spring or graph route;
its reserved navigation words must be zero, like a Fighter's.

Fresh restore builds candidate submarine pointers against the staged world,
checks full water containment and ordinary geometry/actor/other-host clearance,
then publishes the independent bank only after every admission passes. No
setup event, target sample or source handle is replayed from the save. Older
executables reject the newly supported profile4 rather than load it as APC or
Fighter. Original metadata/hash evidence is packaged in
`checkpoint/submarine-source-evidence.json`.

## Remaining code gaps

This slice completes independent authored submarine motion and state transport.
It does not add autonomous torpedo firing, water-route obstacle avoidance,
player switching into an already independent owner, rider transport or
cross-section retention. Those are code gaps, separate from the pending
hourly compile/runtime check. The distinct masako_fighter class is being
implemented separately; no alias to Fighter01 was added here.
