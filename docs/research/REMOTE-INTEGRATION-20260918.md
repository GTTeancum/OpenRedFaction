# Remote-charge gameplay integration

Developer-room weapon slots8/9 now select the charge/detonator with a shared base charge ownership bit and ammo10 reserve. Both authored first-person models load, using inverse authored offsets and70-degree FOV. The projectile pool draws rmt_explosive rather than the carried third-person charge model. No magazine/reload is synthesized for these reserve-only weapons.

Primary throws after21 simulation ticks; alternate slow placement releases after15, with45-tick cooldown. The placement speed2m/s remains port policy. A fixed32-charge pool consumes one reserve round only on successful launch. Charges keep simulating off-weapon; detonator input requests owned active charges, producing one explosion each on the next tick. Actor and mover attachments follow host pose; missing hosts retire and dying actors detach. Detached-fragment attachment transport and save persistence remain open. Explosion damage500, blast radius5 and crater radius5 use the authored charge definition; presentation currently reuses rocket impact resources.

PC replay artifacts/remote-live/remote.bin selects the charge, throws at the pillar, retreats, switches to detonator and fires. Diagnostics report starts1/releases1/attachments1/detonations1/live0 with no error or pool exhaustion; reserve20->19. GeoMod reports one successful cut. The pre-detonation replay charge.bin reports one attached live charge. Both native PC raster outputs were inspected: visible stuck yellow charge, charge-hand pose, detonator model, explosion smoke and displaced pillar debris. Player survives the retreat replay. These endpoint captures do not establish every intermediate animation frame or moving-host visual fidelity.

First-person loaded payloads: charge719280 resident/730876 peak bytes, detonator745464/757060. Xbox endpoint headroom must be measured separately; these counts do not represent total or peak system memory.

## Xbox validation
Native run render-20260918-091615 passes 79 checks over330 frames on stock64MiB. REMOTE matches PC exactly [1,1,1,1,0,0,0,0], with one successful terrain cut. Endpoint available pages: 2720 (4KiB each; not a peak-memory measurement). Native framebuffer inspected: detonator,19 ammo, intact player health and explosion/debris at the pillar. Disc restoration succeeded.

## Save format preparation
RFRM1 explicitly encodes at most32active charges and throw timing in at most7344bytes, with durable identity keys for later host/owner resolution. RFCP2 adds an optional validated RFRM chunk to the existing player/GeoMod container. Its new preflight also accepts RFCP1 as remote-absent; legacy APIs remain unchanged. Total RFSG size cap is unchanged. Focused remote roundtrip/rejection, legacy composed codec and composed-remote tests pass. This is format preparation only: scene save/restore publication and durable key remapping are not connected yet, so active remote charges still are not persisted by gameplay saves.
