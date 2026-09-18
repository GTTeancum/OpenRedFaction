# NPC rockets

The DEV-room enemy scheduler now launches an eight-slot pool of real rockets with authored speed, lifetime, collision radius and AI-scaled damage. Successful launch owns one magazine debit; the ordinary readiness/reload/fallback path remains active and the generic hitscan path is bypassed. Flights retain their source after shooter death and update independently of the equipped player weapon. The shared rocket draw pass includes NPC flights without another model allocation. Active flights block scoped checkpoints.

A shared NPC projectile sweep covers world geometry, movers, detached geometry, other living NPCs and the player while excluding the shooter. NPC grenades use its solid-only wrapper. Rockets use liquid crossing and direct impact followed by radial damage and terrain destruction, retaining shooter attribution.

PC and Xbox builds pass. Focused collision/flight checks pass after supplying the required dry-liquid descriptor in the empty test world. Actual enemy scheduler checks prove one launch, exactly one magazine debit, retained source and no instant hitscan damage on launch or the following frame. No live enemy rocket encounter has yet been verified. Campaign resource loading, predictive aim and authored attack timing remain outside this first pass.
