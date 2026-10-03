# Regular Fighter ownership switching

Regular Fighter01 now participates in the same quiescent ownership exchange as ground vehicles and submarines. The target must remain visible, alive, unoccupied, ungrouped, unattached and free of active/future route or support dependencies. Masako's separate model is excluded. This does not broaden the boot-time Fighter whitelist; L13S3 Fighter8955 and L18S2 Fighter10066 keep their established selection.

Resource packs retain the actual cockpit, muzzle tags, finite minigun/rocket definitions and flight parameters, borrowing the stable chassis. The existing4MiB auxiliary budget and16MiB admission reserve remain. Fighter rocket VFX demand is now prepared outside DEV mode before pack memory admission. Transfer rejects live bullets/rockets or active firing clocks; the outgoing owner retains its ammunition. Incoming Fighter hull and player path require dry-room admission and static clearance before any registry exchange.

RFSW4 preserves the24-byte header and320-byte rows. Profile5 uses the existing primary ammo/scheduler and secondary reserve/cooldown fields; previously unused aim words180/184/188 hold rocket warmup/held/shot count. Fighter aim fields and RNG are zero. Versions1–3 retain their existing meanings and limits. Boot restore recognizes exact Fighter01 identities before resource preparation. Fresh-load/return promotion remain unverified until a native continuation is recorded.

## Focused Xbox evidence

The194149 stock64MiB source ran380 frames: ordinary Fighter boarding, six minigun launches (900 to894), exit, Fighter8955 to Jeep7629 transfer, one Jeep shot (999 to998), and a15768-byte occupied save. Handles33358332/33292795 stayed with their owners, rocket reserve20 remained distinct, and5452 physical pages remained available. The full check failed because the active Fighter incorrectly began with900 class health instead of its placed50, which the transfer faithfully retained. The unused rocket scheduler also has the same settled negative cooldown as the primary scheduler; the validator now permits finite nonpositive cooldown rather than requiring raw zero.

The health discrepancy exposed an existing active-owner initialization omission. Original464010 applies every finite placed health/armor except exact−1, clamped to class limits (ENTITY-INSTANCE-VITALS.md). Passive vehicles already apply it. Active runtime initialization now stages the same override before registration, with exact UID/class matching and unchanged shared class prototype; DEV hosts without an authored UID retain class defaults. Native194653 confirms the fix.

No images, campaign traversal, rocket expenditure or rendered/audio correctness is claimed. The testbed preserves original vehicle metadata except transforms and uses unchanged dry CTF06 geometry.


`vehicle-fighter-switch-20261003-194653/report.json` PASS:380 frames on stock64MiB, Fighter50 health and Jeep400 preserved; six Fighter minigun launches leave894 rounds and20 rockets, followed by one Jeep launch leaving998. Ordinary save retains both owners through RFSW4/RFVA2 with exact poses, vitals and settled firing state (15768 bytes). Available memory5452 pages (21.30MiB). The restored standard NXDK build also succeeds. This is source switch/save evidence; fresh-load, reverse Fighter promotion and rocket expenditure remain open.
