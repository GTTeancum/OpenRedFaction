# Gameplay controls integration

## Enemy melee
Enemy melee now intersects the intended target body along a bounded strike segment and queries existing world/fragment cover before target damage. Cadence and swing presentation remain unchanged. This is a practical first-pass contact test, not weapon-bone animation reconstruction. The focused scene fixture proves reach, off-axis miss and finite mover cover. The existing fragment path is reused; live rubble behavior is not independently verified here.

## Shield event76
Activate_Capek_Shield dispatch uses a dedicated runtime callback and context for registered actor links, including delayed ON/OFF. The scene callback clears actor flags_814 bit0x20 on enable and sets it on disable, matching the recovered event behavior. Existing damage admission consumes the flag. It does not refill health/armor. Runtime tests cover propagation and delayed dispatch; scene tests check the actual immunity predicate. Presentation, ambient ownership and direct shield-hit armor consumption remain open, so this is not complete Capek behavior.

## Remote charges
Shared remote_charge core is registered in PC and Xbox source lists. It implements gravity/sweeps, first-contact sticking, owner-specific detonation requests, availability and once-only explosion events. Host binding stores local pose; update transports position/orientation, detaches dying hosts and retires missing hosts without explosion. Focused numeric tests pass. The scene launch/input/draw/blast integration remains pending; no playable remote-charge claim yet. Contact-before-request original fuse ordering and orientation up-axis are documented implementation limits.

## Validation
PC gameplay executable and stock64MiB NXDK Xbox build pass. Focused scene_nano_shield, event_nano_shield, scene_ai_melee_contact and remote_charge checks pass. This integration batch has not been run in XEMU; build success does not establish rendered gameplay behavior. Logs: artifacts/precision-live/gameplay-build.log, shield-build.log, gameplay-xbox.log.
