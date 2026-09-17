# Authored source selection, 2026-09-17

The authored decoder now accepts explicit ctf06 room3 sources93,94,96,97 through open_source/decode_source. Existing entry points retain94, including the current scene and checkpoint identity. This is one-source ownership, not simultaneous world destruction.

## Asset evidence and admission

All four posts have six convex faces, four compiled visible windows, y bounds[-1.5,2], and an earlier air brush66 plus floor71/ground70. Posts93/94 require beam95;96/97 require beam98. Their x bounds are[-5.25,-4.75] and[5.75,6.25], respectively. Odd entries93/96 have z[-2.75,-2.25];94/97 have z[2.25,2.75]. These bounds are asserted from the installed editor stream transformed by the existing original-derived loader, not screenshot comparisons.

Unsupported source79 is rejected. No extra intersecting brushes are ignored: the existing exact neighborhood, convexity, eligibility, compiled material and face ownership checks remain active. Provenance now carries the selected UID. This does not implement arbitrary editor CSG.

## Verification

All four sources load with3904 resident bytes and1038151 peak decoder bytes on PC. Each receives a real template cut at y=-.9 on its positive-x side and z center, then runs production publication. Each produces35 faces/150 vertices, including8 floor pieces. Tests verify source/window provenance, bounds, hidden horizontal caps remaining unpublished, and floor ownership/material/reference. The original94 exact result remains checked.

PC build and121/121 CTest tests pass. A subsequent focused rebuild/test passes the broadened hidden-cap check. Stock-profile NXDK compilation succeeds, with the existing .edata merge warning. No new emulator session, runtime multi-source claim, or visual acceptance is made.

## Next integration

Replace the scene's single authored terrain owner with bounded multiple ownership and dispatch cuts by the actual impacted source. Preserve per-source identity in checkpoints, rollback and retirement accounting. Then exercise new live chunk extraction after earlier rubble collection. Current source-selection coverage alone does not prove that flow.

## Live selected-source integration

Developer setup now calls rf_scene_authored_post_place_source before opening scene resources. PC accepts RF_REPLAY_AUTHORED_SOURCE=93/94/96/97 in ctf06 DEV setup. Xbox accepts optional four-byte little-endian authored-source.bin on D:, rejecting unsupported values and non-four-byte files. Absence retains94. This translates the existing test spawn by authored post offsets; it is a developer fixture, not original player-start reconstruction.

Identity tests prove repeatability, pairwise-distinct source and published-window hashes, valid chart references, unchanged original94 digest and atomic unsupported-source rejection. Original materials/charts are captured before renderer remapping.

Run `python -B tools/check_selected_authored_sources.py`. Thirteen process-local PC runs cover a rocket cut on each source,201-frame resumed runs versus uninterrupted control (dropping the unconsumed final input at the save boundary), and a93 save rejected in97 setup with `GEOMOD_CHECKPOINT_ERROR load -2`. All four complete checkpoints match their uninterrupted controls exactly. Each creates one live chunk without registry errors;93/94 report12 drawn triangles and133780 resident piece bytes;96/97 report18 triangles and133108 bytes. Individual authored owners report15060 resident bytes and1894363 peak setup bytes.

The PC endpoint renders for93 and97 were inspected: broken post, detached textured chunk, room and weapon/HUD are visible.97 also shows low player health after the nearby blast. No full animation or audio acceptance is claimed. No images were uploaded to GitHub.

All121 CTest tests pass and stock-profile NXDK build succeeds. Live selected-source Xbox execution remains unverified. The scene still owns one selected source, so simultaneous-source integration remains open. No source owner is swapped beneath a live scene or existing save.
