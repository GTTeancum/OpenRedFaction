# Weapon checkpoint progress

Heavy Machine Gun and Precision Rifle now use the existing RFPL inventory/selection fields inside RFCP4. The live scene catalog admits these two guns while Machine Pistol and Undercover remain rejected until their persistent modes are integrated. Pending firing/reload and other existing live-save guards remain.

PC fixtures use real Give_Item_To_Player events, normal weapon cycling and firing, then save and restart into the same scene without dispatching the grant again. HMG reload retains97 loaded rounds and continued fire reduces that to95; Precision Rifle retains18 and then consumes two rounds to16. Reserve values and selected weapon agree with the saved bytes. Restored HMG and continuing rifle output images were inspected. Original CTF06 geometry,506 props and pickups are retained. Artifacts: `artifacts/firearms-checkpoint-live`.

Frame-zero DEV selection no longer overrides imported selection. Spread RNG is initialized for all profiles; its durable continuation is prepared below but not yet wired. The DEV refill chord now refills owned weapons only, avoiding invalid ammunition in unowned weapons whose view resources happen to be loaded.

## Special weapon mode preparation

RFWM1 is a32-byte portable component for Machine Pistol alternate magazine mode, Undercover suppressor attachment and conventional spread RNG. Its little-endian encoding validates catalog, flags, checksum and reserved bytes before publishing output. It allocates nothing. Caller integration still must validate ownership, settled transitions and view resources, then restore modes after normal frame-zero resets. The codec alone does not enable these weapons in live saves.

Native HMG load/fire passes180 frames in `artifacts/xemu/render-20260918-213611`: weapon slot14, two shots, loaded ammo95 and all player checkpoint words match PC. The13,276-byte recaptured PC/Xbox saves have identical SHA256 `1acfc711490a01c10102c23f7229981fd56882d9ade27609137fb28837c8d0c9`. Native framebuffer inspected;2082 free pages (8.13MiB), stock64MiB, original disc restored. Audio was not auditioned. Native Precision Rifle continuation remains unverified.

RFCP5 framing is prepared and passes focused tests: it appends mandatory RFWM32 after RFPC, keeps the110,524-byte maximum, validates before publication, and reads legacy1–4 with modes explicitly absent. Old APIs reject5. Existing prop/vehicle framing checks pass; NXDK builds. Live scene saves still emit RFCP4 until the Machine Pistol/suppressor ownership and resource adapters are connected.

Campaign persistence remains open; this is the existing static developer-room checkpoint profile.
