# Collecting AI: bounded implementation preparation

This is prop hauling, not weapon/ammo collection. The destination helper is implemented independently; live collecting gameplay is not implemented by this change.

Evidence comes from the installed RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836, inspected directly without running the original game. The local Dash Faction `rf/ai.h` supplies corroborating submode names and `rf/clutter.h` identifies the clutter list sentinel at005c9360.

- 004053a0 dispatches submodes4 patrol,5 pickup,6 move to drop,7 drop. Patrol invokes00405460, pickup00405540, drop004058e0; move to drop advances the existing navigation path.
- 00405460 scans the clutter list (head005c95ec, next offset28c) every400ms. Eligible object flags7c contain1000 and lack2000. The first eligible object within40 world units with a clear498e80 query is selected. Selection reserves flag2000; this is not nearest-object selection. Clutter construction4109d5 maps class flags bit0 into object1000. The exact authored class property name is not yet established.
- 00405540 approaches the target, using target position plus1.1 height, with pickup proximity0.5. Animation23 begins when supported, with a5500ms attachment timestamp. Attachment427240 uses the actor attachment array8cc slot0 and sets object flag100. Missing animation takes an immediate attachment path. Animation completion selects the drop destination; failed navigation switches to catatonic mode1.
- 004057f0 scans all events, accepts runtime type27 (`Drop_Point_Marker`), and selects strictly smaller full3D Euclidean distance. Equal distances retain the first event. Position comes from event40 and is copied into AI490. It applies no link or radius filter. No marker returns-1. Distance helper4faed0 subtracts vectors then calls40a000 (square root of sum of squares).
- 004058e0 starts drop animation22 when supported, schedules125ms release, detaches using427380, clears the target, and enables dropped-object physics flags1a8 with80000001. Animation completion returns to patrol4. There is additional post-release object handling through410c70 that remains to be reconstructed before live integration.

`scene_collecting_destination_find(events, origin, result)` uses owned level events directly and returns index, UID, copied position and distance. It allocates nothing and preserves output on failure. It uses double intermediates for portable finite3D distance and explicit malformed-input rejection; it does not promise x87 rounding parity at extreme/tied near-equal coordinates. Tests cover nearest selection including height, deterministic exact ties, unrelated event rejection, no markers, and atomic malformed-input failure.

Integration still requires eligible movable clutter registration/reservation, actor attachment slots and carried-object transform propagation, pickup/drop animation binding, navigation completion transitions, release physics, and cancellation/death cleanup. Do not admit mode5 merely because a destination exists. Parent can include the helper near AI gameplay adapters and add the focused test target linked to rf_core; no scene wiring or build files changed here.

Parent verification: `rf_scene_collecting_destination_tests` builds and passes on PC. This is a helper check only; no live hauling or Xbox hauling claim is made.
