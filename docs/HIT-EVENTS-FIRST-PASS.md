# Hit-triggered gameplay events

The live combat path publishes the shared hit flag on registered actors and props. When_Hit observes its linked targets without consuming that flag and dispatches linked event/mover outputs. The complete event pass clears the flags afterward. Lethal NPC hits remain registered long enough to be observed; protected props publish a hit before protection rejects health damage.

`tools/check_hit_event.py --run` constructs a controlled L1S1 graph using the original guard and geometry. Goal_Create declares `hit_seen`; When_Hit links the guard to Goal_Set. Two 90-frame process-local replays compare no input with one real pistol shot at frame30. No manually triggered event or injected hit flag is used.

PC results: baseline has zero shots/hits and goal0; the shot case has one shot/hit, a surviving guard and goal1. The remaining neutral frames show no duplicate activation. Both endpoint captures were inspected and retain the living guard, environment and weapon. The goal state is verified from runtime output, not inferred from the images.

Scope: this proves a controlled actor-hit event chain. Authored destructible-button encounters, moving outputs, same-pass event-generated damage ordering, full campaign scripting and audio remain separate. No new runtime code was required for this flow.

Xbox verification: `artifacts/xemu/render-20260922-184619/report.json` passes 90 frames with exactly matching combat words and `MISSION_GOAL hit_seen 1 0`. Native framebuffer inspected; 5,244 pages (20.48 MiB) remain available and the original disc was restored. This confirms the event-state result on both backends, not a visible mission HUD or authored button encounter.
