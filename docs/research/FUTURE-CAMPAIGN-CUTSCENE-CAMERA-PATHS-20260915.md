# Cutscene cameras and cubic paths

## Executed evidence

`tools/future_re/campaign_cutscene_camera_paths.py` parses all camera/control/path sections for12 installed cutscene levels with exact byte consumption, resolves every timeline camera UID, and runs **105 actual cubic samples** through original530160 and530060, including52ff10 basis arithmetic without hooks. Result JSON is `cutscene-camera-paths.json`; verified original RF.exe hash matches companion reports. Resource layouts are recovered statically and independently parsed; the resource loader instructions themselves are not executed by this probe. No port edits/builds/stock process/emulator.

## Three resource sections

| Section | Loader | Runtime storage | Relevant data |
| --- | --- | --- | --- |
|0x400|4659b0 (dispatch460d97..460da7)|644f10 pointer array; allocated0x34-byte camera record|UID+0, position+4, orientation+10|
|0x5000|465b90|644f20 inline16-byte records, count644f08, capped64|UID+0, position+4|
|0x6000|465c80|645328 inline100-byte path records, count645324, capped32|name string+0, spline+8, control pointers+48 onward|

Camera and control-point serialized records both follow: u32UID, u16-length name,3position floats,9orientation floats, u16-length text,1flag byte. Each section starts u32count. Camera loader retains position/matrix; control loader keeps only UID and position, reading remaining fields into temporaries. Matrix primitive52cac0 reorders serialized triples to runtime right/up/forward (disk triples1,2,0), as established in the Teleport_Player report. Camera loader has another branch under global6469d8 for a different loading mode; normal game branch allocates camera records. That alternate mode remains outside this evidence.

Each path record is u16-length name, u32control count, then u32control UIDs.45bed0 resolves each UID against644f20. Loader stores pointers at path+48 onward, then unconditionally constructs from the first four positions using530160(path+8,p0,p1,p2,p3). All15 authored paths have exactly4 resolvable controls. Port must reject missing controls and counts other than4 before constructing; original does not visibly protect those cases. Name duplicates/empty names should not be silently removed because lookup order matters.

## Curve semantics verified

530160(this,p0,p1,p2,p3), ret16, copies four vector positions into this+0,+12,+24,+36 and sets its four internal pointers at+48..+60.530060(this,out,t), ret8, clears destination then sums four weighted vectors using52ff10(index,t).

All105 samples match cubic Bezier:

`P(t)=(1-t)^3 P0 + 3t(1-t)^2 P1 + 3t^2(1-t) P2 + t^3 P3`.

Tests use all15 authored paths and t=-0.25,0,0.25,0.5,0.75,1,1.25. Maximum allowed position error0.0005 accounts for original float accumulation. Endpoints and overshoot execute: **sampler does not clamp t**. Coupled with the scheduler's sample-before-deactivation, this matters for a late frame. Do not introduce endpoint clamping while claiming exact parity; any practical clamp should be an explicit port choice.

The curve stores copied positions, so subsequent control-point position changes do not automatically alter an already constructed spline. Camera matrix initialization is separate from spline position; orientation can be overwritten by the optional per-frame look-at target described in the phase report.

## Authored coverage

All84 timeline camera references resolve in the corresponding0x400 sections. Fifteen paths occur across five levels:

- L6S3: two paths, nine control points.
- L20S2: `mtruck`, four controls; one-point vehicle scene uses it.
- L7S4: six paths,24 controls.
- L14S3: four paths including `path1`, `path2`, an empty name and `return`; seven controls reused.
- L17S4: `blowup` and an empty-name path sharing four controls.

Other cutscene levels have no path resources. Every unresolved timeline path name is exactly `none`; no other authored path dependency is missing. This independently supports treating unresolved `none` as stationary camera timing, while keeping generic lookup behavior.

## Minimal shared implementation shape

Parse these sections into bounded UID-indexed camera/control records plus four-vector Bezier paths, alongside the already recovered0x4000 timelines. Source strings must have bounded byte budgets. During scene setup validate all timeline camera and non-`none` path joins. At point start copy camera pose/FOV and dispatch its optional action. During active movement evaluate the four-vector curve into scene camera position; apply optional look-at orientation afterward. No original pointers, original x86 ABI layout or per-frame allocations are needed, and per-level authored data is small under64MiB.

Outstanding before complete cutscene service: point-start object-kind execution, look-at correction48ac70/entity+7d8, frame-delta clock5a4014, skip/cancel control restoration and authored scene smoke coverage. Static camera, curve, scheduler and lifecycle evidence now exist; none of these probes establishes live audio/actor synchronization.
