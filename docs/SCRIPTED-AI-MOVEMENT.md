# Scripted movement and ordinary combat

Active authored Goto, Goto_Player and Follow_Waypoints destinations now retain ownership of translation until arrival or an explicit Attack order. Ordinary sight-driven pursuit, gunfire hearing, damage retaliation and alarms can alert the actor without discarding that destination. The existing firing path remains available in ordinary/default AI mode; explicit AI Waypoints mode retains its previous separate combat-admission policy. Explicit Attack still cancels the route and owns pursuit. Catatonic suppression is unchanged.

This is a practical port arbitration policy. It separates scripted translation from ordinary threat response without inventing a replacement path or changing navigation geometry. Existing unique-linked Follow_Waypoints acquisition was already implemented in32410c24; the old TODO claim that all unbound route acquisition remained missing was stale. Ambiguous or absent authored route bindings remain unsupported.

## Verification

Actual-scene focused checks issue Goto, Goto_Player and Follow_Waypoints orders, then prove out-of-range ordinary pursuit preserves the complete movement state. In-range combat still launches a real finite-ammo NPC rocket while preserving the route. The actual Attack callback can supersede it. Hearing and actual damage-retaliation/alarm callback checks preserve the route while setting ordinary alert/reaction timing.

`python tools/check_scripted_movement.py --run` uses original L1S1 geometry and guard8456/player9858 records, including the guard's authored Riot Stick. A synthetic Goto910700 points two units along the guard's authored right vector. The player remains neutral; ordinary sight first triggers pursuit. Goto fires at30 through the existing NPC-event entry point. The29-frame baseline shows the approaching guard; the90-frame endpoint shows it turned away and walking toward the scripted destination, with its target retained, actual steps and ordinary alert counters. Both captures inspected.

The native harness now accepts `--goto-uid` and stages/restores the existing campaign-goto.bin mechanism, matching PC timing. No host input or desktop capture is used. An initial local fixture mistakenly dispatched Goto through the setup-only entry point; its RF_FORMAT rejection was a harness error, not a Riot Stick animation failure. It was corrected without changing loadout behavior.

Native `artifacts/xemu/render-20260922-182408` passed90frames with identical movement and enemy combat counters. Its framebuffer was inspected and matches the guard walking away toward the assigned destination. Stock64MiB memory had5244freepages (20.48MiB). The disc was restored and XEMU closed. PC/NXDK builds and focused checks pass; this native harness does not separately compare the SCRIPT_ACTOR coordinate telemetry.

## Remaining integration

Full route arrival/loop/reversal encounters, richer moving fire/aim presentation, collecting AI, turret/vehicle transitions and general campaign disk persistence remain. Focused ownership checks are not proof of complete campaign pathfinding. No audio audition performed.
