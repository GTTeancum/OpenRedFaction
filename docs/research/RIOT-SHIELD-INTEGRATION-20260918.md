# NPC riot shields

Level loading parses authored riot_shield durability (1250) and allocates20bytes per NPC plus one shared definition under64KiB. Shutdown releases the owner table; registration-handle changes reinitialize owners. The adapter borrows the rendered held model and hand transform, tests real triangles before the actor contact, then applies the recovered strict front-facing gate and clutter damage factors. Break removes ownership and switches to an owned usable weapon while preserving scripted Attack target/order. No ammo is granted.

Player ordinary and precision gunfire plus NPC ordinary/precision rays and shotgun pellets invoke interception before body damage. Radial, environmental and scripted damage bypass the adapter. Rail retains its existing continuation through later targets. Generic unsupported held items now stop before the enemy gun scheduler, preventing shields from firing invented hitscan attacks.

Focused adapter and scripted-break tests pass; PC builds pass. The mode5 PC fixture loads and renders a shield-bearing guard, but its single head-height shot does not intersect the blocking surface, so live interception is not yet demonstrated. The rendering was inspected. Mode5 is currently a PC-only fixture.

Remaining: live blocking/break demonstration and Xbox encounter, shield contact outside body silhouette, player-held shield, pickup/refill and persistence. The adapter is level-local and does not claim persistent durability across visits.

Follow-up: the original env_guard fixture has no authored shield weapon-motion group. Mode5 now selects miner1, which has the authored shield stance, and faces the opposite direction for front-side staging. The process-local replay aims at the fixture UID0x70000001 (not its original campaign UID) and pulses handgun shots. It still records body hits and death without interception; this is an unresolved live adapter/placement/contact issue, not evidence of working blocking. The final rendered death frame was inspected. PC/Xbox builds and the new actual scheduler test for unsupported held weapons pass.
