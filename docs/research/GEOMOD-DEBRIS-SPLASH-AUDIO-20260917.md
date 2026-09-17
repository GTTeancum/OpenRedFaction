# Debris water-entry audio

The scene now requests `Medium Water Splash` at the reverse-interpolated
water entry point before publishing its ripple. Original48f900 requests
water_splash_medium through4c16e0; the installed binding/definition is
recorded by tools/verify_projectile_liquid_assets.py. Existing emitter
research (secondary-re/campaign-capek-emission and audio-adapter) establishes
434da0 selection and5056a0 positional playback before visual allocation.

The adapter borrows the debris random state, reuses the Foley group selector
and lazy PCM/device services, requests volume1/category0, and retains authored
sample loop metadata. It does not use the player-routed combat sound helper.
Missing sound/voice failures are counted and cannot cancel ripple publication
or chunk movement. This is the existing scene audio adapter, not a claim of
reconstructing the original global RNG scheduling across every game system.

`rf_scene_debris_splash_audio[9]` records requests, selections, starts, loads,
bytes, last sample, last selection RNG, failures and last sample name hash.
It resets with the debris owner and DEV reset. PC/native harnesses expose and
compare all nine words.

Validation: the 550-frame two-shot ctf06 replay produces35 requests,
35 selections,35 starts, zero failures and two PCM loads totaling17362bytes.
All35 crossing endpoints/request points still match actual original48f900
execution, with the original world query supplied as a miss. Five wet births
also match. The16 existing audio/debris/liquid CTests pass. Audible output,
spatial listening quality and voice-exhaustion scenarios remain unverified.

Stock64MiB native run `artifacts/xemu/render-20260917-003922` passes all63
state comparisons across550frames, including identical nine-word splash
telemetry. Endpoint free memory is4158pages (16.242MiB). All246 captured
ripple vertices (3444words), camera, sources, local and placed inputs match
PC exactly. Native endpoint framebuffer inspection confirms the room,
damaged post, weapon and HUD; individual transient effects are not visually
accepted from that single image. The harness restored the disc and exited.
