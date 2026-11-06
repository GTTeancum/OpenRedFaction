# Two-player split screen (PS2) — design

Status: design, 2026-11-05. Nothing implemented yet. Two players only
(the PS2 limit); the engine's quarter-screen layouts are never used.

## PS2 behaviour (traced)

- Mode popup "Player vs Player" (1e3cd0 area): requires a second controller
  (2389a0(1)); without one it shows an error popup; with one it sets two
  humans (1dda20(2), `2ab9e8`) and enters PLAYER NAME (0x17).
- Player records `2ac2b0` (player 1) and `2ac2b4` (player 2), count `2ac2b8`.
- Viewports (1a5ba0, set at match start 113253): player 1 case 2 (top half),
  player 2 case 3 (bottom half); 90 degree field of view (0x42b40000) for both.
  Cases 4-7 (left/right halves, quarters) exist in the engine but the game
  never uses them.
- Menus already traced have two-human variants: PLAYER NAME and CHARACTER
  panels side by side (26bdc0/26ca40 with x0 per player), MP OPTIONS list
  290ee0 with "Player 2 Options" (rows at 0x106/0x11a/0x142), arena bot limit
  = first `$Num bots` value (4 on most arenas).
- HUD: split-screen offsets in 1a9c20, 1abd00, 1acc70, 1af170 and the
  multiplayer parts of 160f40 (not yet traced); the respawn prompt sits at
  y 175 instead of 350; the end screen draws a divider.
- Bots treat both humans as players (12b830 "no human players" = neither of
  the two records).

## Port today

- One player: about 400 player globals (rf_scene_actor_*, actor_*,
  campaign_player_*, scene_player_*, rf_scene_player_*) with about 1,800
  references across scene.c and its .inc files.
- scene_frame (scene.c) interleaves the player's movement and ground
  handling, the world and NPC update, then builds one mesh of draws culled
  for the player's view and hands it to one presentation sink, which flips.
- The player body is a collision participant (scene_actor_body,
  campaign_player_object) that NPC targeting, hits and pickups refer to by
  one handle.

## Design

1. Player context: gather every player-owned global into per-player storage
   (a context block). The live globals stay where they are; a swap copies one
   player's block in and the other's out, so existing code is unchanged.
   Shared world state must never be in the block. Registered objects need
   one registration per player, so player 2 gets its own entity view and
   handle, and the swap exchanges the registry pointer target as well.
2. Frame split: scene_frame is divided into a per-player phase (input,
   stance, ground, movement, weapons, damage, pickups, camera) and a world
   phase (NPCs, projectiles, movers, effects). Each frame runs the world
   phase once and the player phase once per human with that player swapped
   in.
3. Two bodies in the world: collision, hit tests, pickups, explosions and
   NPC targeting must see both player bodies (the places that use
   scene_actor_body / campaign_player_object.handle directly take a list).
4. Two views: the draw preparation (visibility, NPC/clutter/weapon draws,
   first-person weapon) runs per player into the shared mesh buffer, and the
   presentation sink draws each into its half (top/bottom, 90 degree field)
   without flipping, then flips once. In widescreen the pair is pillarboxed
   to a 4:3 area.
5. HUD per half from the traced PS2 split offsets; input from controller 1
   and 2; MP STATS, end screen and scoring for two humans; Player vs Player
   menus.

## Stages (each builds, is committed and smoke-tested)

1. Inventory the player globals and build the swap with a self-test (swap
   twice per frame with one player: behaviour unchanged).
2. Split scene_frame into world and player phases (one player: unchanged).
3. Player 2 body, registration and spawn; both bodies in collision and
   targeting; player 2 driven by controller 2 (or the harness).
4. Two views top/bottom with pillarboxing; per-player HUD.
5. Player vs Player menus, two-human options list, MP STATS and end screen.

Main risks: a missed global leaks state between players; memory (a second
first-person weapon and HUD state within stock 64 MiB); frame rate with the
world drawn twice (each view is half the pixels, but draw preparation
doubles).
