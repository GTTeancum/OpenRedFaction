# Settled Riot Shield and Fusion saves

Ordinary Xbox saves now admit Riot Shield and Fusion Launcher ownership. The
player inventory already persists ownership, loaded ammo and reserves. The
shield's authored magazine32 is not ammunition: the checkpoint catalog treats
it as an ammo-free weapon with loaded0 and stores durability separately.

RFWM2 keeps the32-byte weapon-mode payload, using its former reserved word for
finite positive shield life and flag4 for shield ownership. Saves without a
shield still write RFWM1; the reader accepts both versions. Restore requires
matching ownership, loaded shield resources and life within the authored limit.
Missing/invalid durability rejects before world publication. A saved damaged
shield remains damaged, switching weapons does not repair it, and subsequent
breakage uses ordinary inventory removal and fallback selection.

Shield bash and Fusion reload/cooldown must settle before saving; an active
Fusion projectile still blocks saving because it has no checkpoint record yet.
Restore resets current bash and Fusion input/flight state before continuing the
saved timeline. This is a first-pass settled-state implementation; persistence
of active Fusion shots and pending shield swings remains open.

## Verification

`tools/check_xbox_weapon_modes_codec.py` executes the compiled NXDK codec and
checks RFWM1 compatibility, RFWM2 durability round trips and rejection of zero,
negative, NaN and unowned durability without publishing partial output.

`tools/xemu_special_weapon_save.py` uses stock64MiB XEMU and a private HDD.
Its process-local fixture grants both weapons using the ordinary item service,
applies an admitted shield hit, saves, boots again and loads, then damages and
breaks the restored shield before firing the Fusion Launcher. Shield contact is
pre-admitted, so this checks durability/save/gameplay integration rather than
shield mesh interception. No PC gameplay, campaign traversal or images are used.
Stock64MiB report
`artifacts/xemu/special-weapon-save-20260930-111454/report.json` passes: the30308-byte
world save restores shield life625 (half of authored1250), the next admitted hit
reduces it to312.5, and a lethal hit removes ownership and selects pistol slot0.
The saved Fusion shell then launches once and loaded ammo reaches0. The load
retains4478 free pages (17.49MiB), and all disc flags are restored. Final NXDK
build and compiled codec checks pass. This check does not cover in-session load
while a Fusion shot is active, Fusion impact presentation or live shield geometry.
