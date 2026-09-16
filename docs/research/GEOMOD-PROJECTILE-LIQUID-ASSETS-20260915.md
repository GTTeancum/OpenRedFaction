# Projectile liquid effects (2026-09-15)

`tools/verify_projectile_liquid_assets.py` executes original initialization4c1323..4c1369 and actual name resolver4c1d00 against the installed vclip table. It verifies global8568a8 binds slot45 `bullet_splash`, and8568b0 binds slot42 `water_splash_medium`. These are the two choices consumed by original liquid handler4c4e30, distinct from the large/huge physical-object contact splashes already retained by the scene.

Both definitions load through the shared C asset loader and name `WaterRipple01.VFX`, authored radius3 and flag1. Foley differs: `Bullet Splash` versus `Medium Water Splash`. Owned definition hash is1527151662. This verifies resource identity/metadata only; VFX instance rendering and audible playback are not implemented or visually validated by this probe.

The liquid policy must not queue GeoMod or solid impact blast effects. Original subtype1/2 chooses bullet_splash; other subtypes choose water_splash_medium. Do not substitute the already loaded large/huge contact effects merely because they are available.
