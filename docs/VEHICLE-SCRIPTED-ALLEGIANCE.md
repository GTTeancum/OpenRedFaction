# Scripted vehicle allegiance

## Source and scope

Original L1S3 APC26 is authored friendliness1, health900. Event9619 is an
immediate Set_Friendliness with word0=0 and links26/9449/9452/9454/9455.
Event9636 has the same value0, a3-second authored delay and links26/9639.
These events repeat the same change; neither is an opposite-team toggle.

The original action audit `tools/verify_door_event_actions.py` documents
4bc280 ->489f70/4895f0 writing the full word to+1f8, including patterned
values. The original OFF dispatcher4b9f80 is a no-op. The original executable
is not present in this cloud input set; that historical binary audit was
reviewed, not rerun. Event/actor evidence is decoded from current original
levels1.vpp bytes. No new hostility values or team equivalence are invented.

## Runtime

The scene callback resolves selected and passive vehicle owners before
skeletal entity lookup. A passive chassis is a kind11 general object, with
no entity view; its full registry handle and damage handle must both match.
A selected host also requires the current entry registration and existing
selected-owner validation. Only the stored damage affiliation changes.
Health, physics, routes, visibility, occupants and full handles are unchanged.

Secondary motion uses the same passive damage owner. Ordinary switching
transfers the complete damage owner; the parked RFSW bank has no separate
team copy. The existing occupied damage-source override still follows the
player driver's affiliation without rewriting the stored host affiliation.
NPC/turret behavior and automatic hostility predicates are unchanged.

## Ordinary saves

Optional RFAL1 sits inside RFNS and outside RFPV/RFSV/RFVA. It leaves all
existing inner codecs unchanged. The16-byte header stores magic/version1/
inner-byte-count/override-count. Each8-byte row stores authored UID/full
uint32 affiliation. Only owners differing from their authored value are
written; if none differ, no new bytes are emitted.

RFWC already checks source identity and checksum. Preparation validates
lengths, budget, complete fresh ownership, UID uniqueness, registration,
unknown/duplicate overrides and all source records before changing anything.
Rows bind fresh live pointers locally, never serialized handles or pointers.
All other owners receive authored defaults. Final assignment runs after
selected, passive, switched and secondary publication. Legacy ordinary saves
have no RFAL and retain their existing authored-team behavior. Legacy RFCP
capture refuses changed vehicle allegiance rather than dropping it.

## Focused acceptance

NXDK/Clang19 builds with the stock memory profile. The production callback
and codec pass42 focused C checks: selected/passive full-word writes, repeated
values, stale/mismatched registrations, unchanged owner guards, legacy exact
bytes, sparse capture, UID ownership rebinding, source/duplicate/length/budget
rejection, empty inner rejection and no live mutation on failure.

`tools/xemu_vehicle_allegiance_events.py` keeps original L1S3 APC26/9627 and
both Set_Friendliness records byte-exact. Unrelated actors/controllers/triggers
are isolated; an ordinary Delay/Invert graph sends OFF, then a0.75-second ON
to9619 while original9636 retains its3-second delay. No vehicle route is run.

Two serial Linux XEMU0.8.136 boots each complete120 frames with exactly
67,108,864 bytes RAM and the player alive:

- Source: frames0/20/40 retain APC26's authored1 with no friendliness callback,
  proving OFF is a no-op. From frame58 the original9619 callback has changed
  only APC26 to0. Health900/armor0, pose, flags and full handle1245202 are
  unchanged. Selected hidden APC9627 retains health5000/team0/handle1310739.
- The ordinary4,224-byte RFWC saves one RFAL1 override[26,0] and retains the
  original9636 deadline with1,017ms remaining. Inner RFPV2/RFVA2/RFVC2 remain
  unchanged. The player has its separate handle1179665.
- Fresh load: frame0 restores the exact saved owner sample and changed team0
  before any friendliness callback or setup replay. The remaining deadline is
  correctly anchored at1,001ms after the first16ms tick. Original9636 later
  fires once through ordinary dispatch, repeating0 to0 and consuming the
  deadline. These two authored events are never described as opposite toggles.
- Free-page endpoints are6,420 source and6,260 loaded. Both phases have no
  NPC actors or campaign transition/request. Test disc inputs restore exactly.

Evidence: `artifacts/xemu/vehicle-allegiance-events-20261008-104500/report.json`,
its native source/load results and original ordinary save. The earlier
`vehicle-allegiance-events-20261008-104200` launch failed before gameplay:
a relative run directory made XEMU miss its configuration. The runner now
resolves that path; its isolated inputs were restored before the retry. The
failed attempt remains recorded and is not gameplay evidence.

Compiler stack audit retains131,072-byte reserve and45,212-byte scene-chain
local-frame subtotal. This is not a runtime stack high-water measurement.

## Boundaries

Selected host changes and selected/passive UID transfer are covered by the
production callback/codec checks, not this unoccupied passive-APC runtime.
Occupied driver attribution and whole-damage-owner transfer are unchanged by
source review. The fixture does not establish live moving/occupied team
changes, homing reacquisition, autonomous APC combat, riders, campaign routes
or cross-section revisit continuity. These remain separate gameplay work.
Older saves remain readable; older executables do not understand new RFAL1
payloads. No exhaustive compatibility or retail-fidelity claim is made.
