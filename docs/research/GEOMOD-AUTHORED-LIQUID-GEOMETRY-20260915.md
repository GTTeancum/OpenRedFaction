# Authored liquid surface inventory (2026-09-15)

`tools/inspect_liquid_geometry.py` parses the94 retained static geometry sections with the existing full structural checker, then inventories room liquid markers and face flag4. It finds72 liquid rooms across25 levels and1300 serialized liquid faces. Every flagged face belongs to a room whose liquid marker is set. This demonstrates that the installed authored data supplies liquid polygons; a guessed infinite horizontal plane is unnecessary for these surfaces.

The report records level/archive, room indices and bounds, liquid payload bytes and texture names, plus face indices/owners/flags. Raw payload fields are intentionally not assigned unverified meanings. Results remain in artifacts/liquid-geometry-inventory.json. L2S3 and L3S2 each have one water room and two liquid faces, useful compact integration fixtures; no enemy-free status is asserted.

This is a source-data inventory, not collision acceptance. Original4df5eb..4df65b consumes liquid-marked faces under a separate query/room gate. Room traversal order, nearest-hit publication and collision discriminator must be preserved when adding that pass to the current backend. No original screenshot or native game launch was used.
