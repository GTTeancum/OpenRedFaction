# Weapon checkpoint progress

Machine Pistol and Undercover now join Heavy Machine Gun and Precision Rifle in the live static DEV-room checkpoint profile. RFCP5 preserves their inventory, selection, completed alternate modes and conventional spread RNG. Pending firing, reloads and mode transitions remain excluded from saves; campaign-wide persistence is still open.

## Live PC mode saves

`tools/check_weapon_modes_checkpoint.py` exercises real acquisition, normal firing/mode inputs, a save, a fresh-process neutral reload, and another fresh-process reload followed by firing. All six PC runs pass in `artifacts/weapon-modes-checkpoint-live`; each save is13,308 bytes. Geometry and all506 authored CTF06 props remain intact.

- Machine Pistol uses real `Give_Item_To_Player` events for `Machine Pistol` and `5.56mm_ammo`, then normal cycling, a base-magazine shot, the authored114-tick alternate transition, a reload and a special-magazine shot. RFPL retains base weapon ID9 with29 rounds and the independent special ID10 magazine with19 rounds. It stores base ownership/selection, not separate ownership of the special mode. Reload selects scene slot13 in special mode with19 rounds and180 reserve; continued fire consumes one special round to18. No toggle transition or grant repeats during reload.
- Undercover has no installed weapon pickup class, so the fixture explicitly uses DEV firearms profile4. It attaches the suppressor through the normal alternate animation, fires, and saves attached mode with14 loaded rounds and125 reserve. Reload retains scene slot16, attached suppressor and14 rounds; continued fire reduces the magazine to13. Attach animation and supply grants do not repeat.

The saved RFWM flags are1 for Machine Pistol special mode and2 for Undercover attached mode. The Machine Pistol snapshot retains conventional RNG3884216597; Undercover retains1. The live adapter restores this RNG together with mode state. These runs establish working mode/ammo continuation; they do not constitute a complete uninterrupted-versus-reloaded spread-sequence comparison. The parent inspected both restored PC weapon images and both native final framebuffers: the weapon models and attached suppressor appear in the expected DEV room. The automated report retains its state-only label; audio was not auditioned.

## Framing, ownership and initialization

RFWM1 is a32-byte portable component containing mode flags and conventional spread RNG. It validates its catalog identity, checksum and reserved bytes without allocation. RFCP5 appends RFWM after RFPC and keeps the110,524-byte total checkpoint cap. Legacy RFCP1 through4 remain readable with mode state explicitly absent; older APIs reject version5.

The live catalog admits both Machine Pistol magazines under base ownership, plus Undercover, HMG and Precision Rifle. Before world publication, the adapter checks ownership, required view/custom-animation resources and settled transitions. A special-mode magazine does not become a separately owned or separately selected gun.

On restore, ordinary player import and carry-sidecar resets happen first. The pending checkpoint modes and RNG are then assigned before selecting the saved weapon and publishing its ammunition. This ordering prevents fresh-scene defaults from erasing the restored mode or selecting the wrong magazine. The current alternate-input edge is consumed, and no saved transition animation is replayed. Frame-zero DEV selection and supply refill no longer overwrite imported state.

## Conventional firearms and native coverage

The earlier HMG/Precision fixture uses real `Give_Item_To_Player` events, ordinary cycling and firing, then reloads without redispatching the grant. HMG retains97 loaded rounds and continued fire reduces that to95; Precision Rifle retains18 and consumes two rounds to16. Reserve and selected weapon match saved RFPL bytes. Restored HMG and continuing rifle images were inspected. Artifacts: `artifacts/firearms-checkpoint-live`.

Native HMG load/fire passes180 frames in `artifacts/xemu/render-20260918-213611`: slot14, two shots, loaded ammo95 and all player checkpoint words match PC. The13,276-byte recaptured PC/Xbox saves have identical SHA256 `1acfc711490a01c10102c23f7229981fd56882d9ade27609137fb28837c8d0c9`. The framebuffer was inspected;2082 free pages (8.13MiB) remained on stock64MiB, and the original disc was restored. This is the earlier RFCP4 HMG evidence. Audio was not auditioned.

Native RFCP5 Undercover passes180 frames in `artifacts/xemu/render-20260918-214834`: attached suppressor, one suppressed shot,13 loaded rounds and125 reserve match PC. Both13,308-byte resaves have SHA256 `d57d7a84ebe421bede2fe14e81f69e6957942ff14b7568e7a985b63b5754500b`;3416 free pages (13.34MiB) remain.

Native RFCP5 Machine Pistol passes180 frames in `artifacts/xemu/render-20260918-215023`: special mode, one shot,18 loaded rounds and180 reserve match PC without replaying a mode transition. Both13,308-byte resaves have SHA256 `bbf7c694c26fb9462f7b58801156606e6963bb1cd25462b6f47cfdaeb94317ff`, including continued RNG1030492215;2082 free pages (8.13MiB) remain. Both runs use stock64MiB, restore the original disc, and close their emulator. Native Precision Rifle continuation remains unverified.

## Remaining scope

These are static developer-room checkpoints, not general campaign saves. Campaign event/AI state, live level handoffs and broader gameplay persistence still need integration. The existing static-world, player-placement, health and settled-action guards remain. Broader mode combinations, uninterrupted RNG comparison, native coverage and tester-led refinement remain follow-up work rather than claims of retail completion.
