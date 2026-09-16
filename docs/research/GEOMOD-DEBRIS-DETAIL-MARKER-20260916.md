# Retained debris detail marker: blast exclusion, not proven release (2026-09-16)

The inspected original code does **not** establish an owner-disappearance release or generation check. The concrete consumer found is the existing-fragment prepass in48fe30: any nonzero chunk+68 excludes that fragment from blast relaunch, without dereferencing the saved pointer. Original48fd70 fade/removal also ignores it. The earlier floor report's proposed release risk was a hypothesis, now narrowed by executable evidence; do not add an inferred support-lifetime mechanism on its basis.

## Executed original contracts

`artifacts/future-vehicles-re/debris_relaunch_marker.py` / `debris-relaunch-marker.json`:81 cases execute original48fe30..48fe9b, stopping before new-fragment allocation. Actual distance4faf30, random range504e40, floor573e83, integer conversion573528 and complete490150 launch run, including actual CRT RNG; only the CRT thread-storage pointer is supplied. Normal/RNG/launch helpers are not stubbed.

Inputs cover marker0, mapped nonzero address and deliberately unmapped0xdeadbeef; distance1/2/3 with radius2; bouncecount0/1/4; seeds0/1/ffffffff. A nonzero marker skips unchanged without dereference for every case. With marker0, distance strictly less than radius resets bouncecount to floor(random[3,5)), observed3/4, then relaunches via490150. Exactly three RNG draws occur (one reset draw, two launch-cone draws). Radius equality is excluded. Only velocity+48..53 and counter+64..67 change. **Age, lifetime, spin and flags are not reset**, and no test of incoming bouncecount is performed: active or settled unmarked fragments can relaunch.

`artifacts/future-vehicles-re/debris_marker_fade.py` / `debris-marker-fade.json`:30 cases execute complete48fd70 with actual48f3d0 unlink; pause service and graphics submission are supplied. Marker0/mapped/poison each gives identical age/fade/removal behavior across age2.5,2.75,3.499,3.5 and next-float-above3.5, lifetime2.5, paused/unpaused. At entry age>lifetime+1, actual unlink transfers the fragment from active sentinel75eed8 to free sentinel75f168 and decrements76d01c; marker+68 remains unchanged. At equality, it remains for that call. No owner validation, nulling, generation comparison or restart occurs.

## Field lifecycle and scope

-48faa3..48faba stores face+44 into chunk+68 only for terminal contact on eligible breakable detail; eligibility4e36a0 is already independently verified.
-48fe49..48fe4e consumes only zero/nonzero and skips marked fragments.
-New fragment initialization in48fe30 writes chunk+68=0 (raw export documents this); reuse therefore resets it when a new fragment is initialized, not on unlink.
-48f4e0 only simulates chunks whose signed bouncecount is positive, including a second check after contact before gravity/rotation. Its recovered loop does not inspect chunk+68.
-Render submission517080 routes to558320; disassembly558320..558445 reads geometry/material/pose but not chunk+68.

An executable text search for direct references to active-list sentinel75eed8 found initialization, update, draw, spawning/relaunch, radius fade and pool management entries. The inspected relevant lifecycle functions expose no marker dereference or generation field. This is bounded evidence, **not a proof that no external indirect callback can touch fragment memory**. No detail deletion callback has been executed, so do not claim that a disappearing detail necessarily drops or destroys its resting fragment. The proven fallback behavior is ordinary timed fade, independent of marker validity.

## Actionable next gameplay improvement

Existing debris should react to subsequent blasts: scan the bounded live pool before new allocations, skip detail-marked fragments, strict distance<blast radius, reset bounces with one random draw to3/4 and invoke verified490150 launch with two more draws; preserve age/spin/flags. This is an evidence-backed improvement over inventing detail-release behavior. Current reconstructed chunks have no retained detail marker, so broader detail integration needs an explicit representation before claiming that exclusion is complete. No shared source edits or builds were made for this report.
