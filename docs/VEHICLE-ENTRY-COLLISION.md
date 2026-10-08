# Vehicle boarding and exit collision

Ordinary Use previously copied the chassis world/mover query into the player entry adapter. Nearby NPCs and passive vehicle hulls were not composed into boarding or exit sweeps, so a geometrically clear exit could place the player in another owner.

The entry adapter now owns a separate scene query. It retains static/moving-world collision and adds actual actor/vehicle shapes, clutter and detached terrain. The controlled host is excluded during transit out of its interior; its existing final hull-exclusion check remains. Hidden and retired owners must not obstruct entry. Visible wrecks remain obstacles.

Final standing and swimming destinations also require full actual-body placement checks. A swept head path alone cannot establish a standing body fit. Downward support must come from the world or a mover; an NPC, vehicle or loose piece is not silently treated as an exit floor. Blocked directions continue the bounded existing search, and no available exit leaves occupancy intact.

APC now shares the Jeep's actual-head seated transit and saved-seat placement.
This fixes settled boarding with an authored seat below standing body height;
full standing exit checks remain. The bounded stock64MiB source/save/load,
exit/reboard and real floor-blocker evidence is in
[selected vehicle visibility and APC seating](VEHICLE-ACTIVE-VISIBILITY.md).

Callbacks are installed after runtime publication and restored immediately when cross-class publication replaces the entry collision copy. Chassis motion and physics retain their previous provider. Existing ordinary checkpoint occupancy restores preserve these callbacks.

Stock64MiB Xbox continuation `vehicle-fighter-switch-20261003-200623/report.json` PASS: occupied Jeep load, ordinary exit, return to parked Fighter and six restored-ammunition launches still work with the new collision provider. Available5290 pages (20.66MiB). The final stricter NPC/turret object-registry identity predicates landed after the run build; they are included in the successful restored standard NXDK build. Focused host checks below cover the final source. This is a practical port collision policy, not a claim of recovered original exit ordering or complete crowded-scene coverage.

`rf_scene_vehicle_entry_collision_tests` passes against the final source: actual scene query retains nearer world contact metadata, excludes the player/active hull, admits other passive/NPC sphere contacts, ignores hidden/stale owners, rejects overlapping destinations, and makes the real exit routine reject dynamic support while accepting an equivalent static floor. The executable exercises shared C collision code only; no PC game session or campaign traversal was run. Building it exposed a pre-existing void-pointer arithmetic extension in vehicle save selection; an explicit byte-pointer cast now keeps that read standard C.
