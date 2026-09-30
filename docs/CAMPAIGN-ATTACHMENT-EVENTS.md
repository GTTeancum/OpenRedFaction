# Campaign attachment event evidence

Read-only code inspection of the installed `RF.exe` (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`)
and the installed level inventory. This is implementation guidance, not a
claim that the events run in the reconstructed Xbox game.

`Item_Pickup_State` (type 54) and `Turn_Off_Physics` (type 62) both map through
factory `0x4b69d0` to the base constructor `0x4bee70`. The base ON method is
the empty `0x4b8cd0`; its ordinary link propagation uses `0x4b8b00` and
`0x4b65c0`. That target dispatcher recognizes registered events, triggers,
group controllers and ambient sounds, not placed items. All five installed
`Item_Pickup_State` events link only First Aid Kit placed-item UIDs. They do
not establish a runtime pickup-enabled toggle, so adding one based on the
event name would invent behavior. The eight `Turn_Off_Physics` events mostly
link moving-group keys and use ordinary base propagation; they likewise do
not establish a dedicated physics-off operation.

`Detach` (type 58) differs: factory case `0x4b7299` constructs vtable
`0x589bac`; ON method `0x4bcc50` resolves each linked object, checks its
eligibility and repeatedly finds its parent with `0x46b970`. It unlinks the
child through `0x46ba10`, preserves its world transform and clears the
child's parent reference. OFF uses the base method. The five authored Detach
events link the L5S3 moving submarine (UID 3977), three L20S1 fighter
attachments (1490, 12335, 12369), and an L20S2 hangar-lift child (4717).

The reconstructed runtime currently owns mover memberships from each group's
second ID list. `rf_group_mover_memberships_open` explicitly leaves the first
general-object ID list unbound, and scene controller views have no general
handles. Consequently the parent-child graph that `Detach` mutates is absent.
The next implementation must bind that graph, propagate parent motion to
children, then let Detach remove the exact child link while retaining the
child's current world pose. A callback that only clears a registry bit would
not release these authored vehicles and platforms.
