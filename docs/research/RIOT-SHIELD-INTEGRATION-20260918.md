# NPC riot shields

NPC shields use authored1250durability, real held-model triangles, the recovered front-facing damage gate and clutter damage factors. Per-level ownership costs20bytes/NPC plus one definition within64KiB. Break removes shield ownership and selects another owned usable weapon without discarding a scripted Attack target or granting ammunition.

Ordinary and precision player shots independently select actual shield surfaces, including outside the actor silhouette. Coarse NPC body spheres can protrude before a held shield: when that occurs, current posed NPC triangles determine whether the real body is nearer. Candidate discovery is read-only; only the nearest unobstructed winner changes durability. Rail retains continuation to later actors. NPC ordinary/precision/shotgun rays also invoke shield interception, but those paths still require a body candidate. Radial/environment/script damage bypass shields.

## Live evidence

The original env_guard fixture lacked a shield animation group. Mode5 instead uses miner1, which has the authored shield stance, with a staged pose and recorded process-local aim. Independent triangle math showed a valid shield intersection at fraction0.058064 hidden behind a coarse body entry0.054638. Exact body-model ordering resolves that overlap without widening the shield geometry.

PC650-frame replay shield-native.bin blocks9shots, leaves the NPC unharmed and retains890durability. PC2100-frame shield-break.bin blocks32shots, breaks once at-30durability, then records3body hits and2enemy attacks after fallback. Framebuffers were inspected: shield visible before break and absent afterward. Focused gameplay, query immutability, stale identity, break fallback, unsupported held-item, precision ordering and exact-model query contract checks pass. PC/Xbox builds pass. Native render-20260918-110448 passes the650-frame recorded blocking encounter with exact PC/Xbox shield counters:9blocks,0breaks,890durability. Native framebuffer inspected with intact shield;1991free pages (7.78MiB). Native break encounter remains unverified.

Remaining: native break validation, NPC shield-only silhouette selection, player-held shield, pickup/refill, hit/break audiovisual polish, in-memory history integration and disk persistence. Shield damage is currently level-local. Helpers for revisit history exist but are not yet wired or counted complete.
