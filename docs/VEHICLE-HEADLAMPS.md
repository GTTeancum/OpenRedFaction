# Authored vehicle headlamps

## Original evidence

Read-only disassembly of recovered RF.exe, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- Type53 ON:4b9070 dispatch table selects4b90a9→4b92e0. OFF:
  4b9f80 selects4b9fa7→4ba0e0. Both walk linked entity handles through426fc0.
- ON429560 sets entity810 bit4000; OFF4294a0 clears it. Both walk the
  entity's1418 glare array, test child2b4 bit2 through4154d0, and change
  only marked children:42d8e0/42d8d0 write active28c=1/0. They continue
  through entity200 parents until missing. Moving-group membership is not
  this parent chain. Repeats still apply; no mode, velocity or weapon changes.
- Sound is conditional on42a8e0, the actual local-player owner predicate;
  a vehicle event does not substitute its driver to play a headlamp sound.
- 41c2a5..41c461 reads numbered `$Corona (Glare) N` declarations. Marking
  is by glare-class membership in the separate6c0 list, not exclusively the
  declaration's slot. 424cce..424da6 resolves sequential `corona_N` model
  tags;4234eb..42358b creates valid paired glows with413d20.
- Factory413d20 makes real type10 glare children. Marked children receive
  bit2 at413e8f. Existing reconstructed corona/volume rendering consumes their
  active byte and uses the original class dimensions/bitmaps and actual tag
  orientation. These headlamps do not create guessed world spotlights.
- The second byte in the six-byte block before angles/vitals is read at
  46423b and retained by464246. Exactly1 also sets the authored AI bit10000;
  the vehicle predicate429990/486c90 additionally selects headlamp ON/OFF at
  4649a0..4649be. The new allocation-free parser leaves the spawn struct ABI
  unchanged and validates the whole v180 record.
- Common link-propagation table4b8c40 returns true for53.

Installed event3965 in L5S3 links selected sub3963. L5S4 event3962 links
selected sub3955; event3965 links independent sub2381. All three source
vehicles have the initial byte1 and declare four `Sub Headlamp` glows.
Sub3977 instead has initial byte0. The same exact class/tag path supports
authored Jeep/Fighter/Masako headlamps; unrelated engine coronas are unchanged.

## Implementation and ownership

`scene_vehicle_headlamp.inc` binds each existing authored vehicle UID/handle
to its initial flag and appended, separately registered type10 glare children.
It reuses the scene's existing glare classes, bitmaps, rendering, room refresh,
visibility and retirement. Headlamp metadata, real children and array growth
share the existing256KiB glare ownership budget; the entity-table scratch has
the existing512KiB table limit. No per-frame allocation is introduced.

Each tick resolves the current generation-valid selected/passive owner and
uses its own live chassis resource tags and committed pose. Switching changes
neither the original host handle nor the glare parent. Independent simulation
continues to borrow its passive damage/flags owner. Generic clutter attachment
nodes skip these children, avoiding a fabricated clutter/NPC parent. The
vehicle consumer publishes their real tag poses before ordinary glare-room
refresh and draw collection; hidden/removed parent gates use actual owners.

Event53 changes entity810 bit4000 and the corresponding children immediately.
It accepts represented vehicle ancestors only; general player/NPC headlamp
creation and input toggles remain outside this vehicle slice. Installed
vehicle event targets have no linked entity parent. Entity flag bit4000 is
unrelated to object7c hidden bit4000 or invulnerability bit4.

## Save continuity and status

RFVA2 already saves complete passive flags810, including parked and independent
owners. RFPV4 retains the same16-byte header and72-byte row, storing the selected
headlamp bit in previously reserved row+68. Old versions1..3 still require
that word to be zero; their exact object masks remain. Version4 checks passive
agreement against RFVA2 before assignment. Other flags and physics fields
are unchanged. Source-default ON forces a v4 row even after an explicit OFF.

Bare/legacy selected records restore their source headlamp default before any
ordinary RFPV4 override. RFVA1 without damage similarly restores the authored
default. Restore synchronizes live child active bytes without replaying an
event. Existing per-frame tag publication refreshes their transforms; cosmetic
glare sample/cache state is rebuilt rather than serialized. Legacy RFCP refuses
changed lamp state it cannot represent. Switching publishes view810 from the
real incoming damage owner rather than a stale parked copy.

No build, test, emulator or campaign run was performed. The parent owns the
13:00 UTC batch. Older focused harnesses asserting an exact RFPV2 version for
an authored initially-ON APC need their version expectation updated if reused;
the runtime loader accepts both formats. `rf_scene_vehicle_headlamp[12]`
provides owners, created children, ON/OFF calls, UID/handle, flags810, pose
publication count, enabled children, added ownership bytes and opening status.
