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

Shield bash must settle before saving. Ordinary world saves now carry active
Fusion flight/reload/cooldown state through RFAP2 (below); the legacy composed
RFCP path remains settled-only. Restore resets current bash and Fusion state
before applying the saved projectile timeline. Pending shield swings remain open.

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

## Active Fusion flight and input continuation

The ordinary RFWC3 projectile component now writes RFAP2 when Fusion state is
present. Row6 retains each active flight's slot, position, velocity, collision
radius, remaining lifetime, liquid-query flags, damage and rendering basis.
The player source and authored explosive definition rebind to the loaded world;
no runtime handle or table pointer is stored. Boot resource demand also reads
Fusion rows, allowing a surviving flight to load its resources independently of
current inventory ownership.

Cooldown and reload deadlines are stored as remaining frame counts, shared by
all Fusion rows. Row7 carries input state when no flight remains. Restore checks
selected weapon/ownership and reload ammunition, then rebases deadlines to the
new timeline. Weapon-state resets precede projectile publication. RFAP1 saves
remain readable and are still written when no Fusion state is present. The
older composed RFCP path remains settled-only.

`tools/xemu_fusion_flight_save.py` is a bounded Xbox check: grant two shells,
fire once, begin reload, save during flight/reload, boot and load with neutral
input. It checks exact initial flight/timer restoration, mathematical movement
or a terminal flight event, no duplicate launch, and one reserve-to-loaded shell
transfer. The load fixture makes the player invulnerable through the ordinary
script service to isolate reload completion from the close-range explosion. An
unprotected first run restored the shot and its impact killed the player at
frame21, correctly canceling reload; the protected continuation reuses that
captured save instead of repeating the source session. Stock64MiB reload report
`artifacts/xemu/fusion-flight-save-20260930-112559/report.json` passes against the
source save in `artifacts/xemu/fusion-flight-save-20260930-112154/`:30448-byte
world/128-byte flight component, exact restored position, cooldown29/reload51,
one impact, no fresh launch and reserve/loaded1/0 becoming0/1. Minimum sampled
endpoint headroom is4621 pages (18.05MiB). Disc flags were restored and NXDK
build passed. Controller-only rows, mixed flights, active
in-session rewind, underwater impacts and presentation remain unverified here.
