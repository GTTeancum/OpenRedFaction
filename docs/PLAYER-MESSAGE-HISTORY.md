# Player Message History

Source-only implementation, October12,2026, based on the parent-supplied
frozen `b304abcd` tree. All changes are staged outside that repository for
parent integration after the16:00 build/check/proof. No build, test, fixture,
XEMU, campaign route, saved-HDD operation, image, statistical/hash sweep,
upload or repository write was performed. Compilation and gameplay behavior
remain unverified. This is a first-playable menu policy, not retail UI parity.

## Actual text and producer ownership

- `src/core/event.c:startup_event_action` dispatches authored event type15
  (Message) to `campaign_show_message`; action2 is ignored, and ON/OFF are
  separate existing operations.
- `src/core/level.c:rf_level_message_read` resolves the current level's
  `_text.tbl`; `rf_level_message_parse` supplies the actual English text into
  `rf_level_message.text[512]`, with a terminating NUL. It rejects an oversized
  source rather than returning silently shortened text.
- `scene.c:campaign_show_message` reads that message, starts the existing
  independent voice route, computes the existing reading/audio deadline, and
  publishes `campaign_subtitle`, UID and deadline. Only after all those steps
  succeed does the new history producer copy the text and its level, event UID
  and message ID. Empty text is not an entry. Repeated accepted ON actions are
  separate received entries; there is no invented deduplication or replay.
- OFF, transient expiry, draw calls and voice-completion polling never append
  or remove history. Pickup/save/load notices are excluded. Goal_Create,
  Goal_Set and Goal_Check own numeric mission state, not an authored display
  description; this feature does not synthesize an objective list or claim
  completion status. An objective instruction is retained when actually
  received through the authored Message path.
- While `rf_scene_quickload_pending()` is active, constructor Message callbacks
  do not append history. This leaves existing subtitle/audio dispatch intact
  while preventing a failed reconstruction from evicting the prior ring.

## Player controls and bounds

Start opens pause; Message History is the final row after Options. Existing
Resume/Save/Load/Restart/End Session/Options values0–5 are preserved, and
History is6. The seven-row menu has room for the existing restart-unavailable
notice and controls.

Opening history selects the newest entry and its first page. D-pad up chooses
an older entry; down chooses a newer one. Left/right selects the preceding/next
page. Selection stops at the ends. Changing messages resets the page to0.
B returns to the pause menu with History still selected; Start resumes through
the existing parent pause owner. A does not replay speech or activate a message.
An empty history has an explicit empty-state screen.

The fixed ring holds16 entries. Each retains all512 source-text bytes,64 level
bytes and two32-bit authored IDs. The oldest record is replaced when full,
and the view shows a saturating count of older messages dropped. This is an
explicit recent-history limit, not an unlimited transcript.

The ring is9,356 bytes; its reusable1024-byte expansion and128-byte line scratch
bring the owned static payload to10,508 bytes, guarded at compilation by a12KiB
ceiling. These are source layout bounds, not measured stock64 peak-memory or
stack evidence. There is no history heap allocation, new texture, archive read,
scene copy, unbounded list, saved payload or disk writer.

Display reuses `scene_hud_control_text` without changing retained text or
message/audio timing. On Xbox, $MESSAGE_LOG$ now describes the actual
START / MESSAGE HISTORY route; PC keeps UNBOUND because this work adds no PC
menu. Unknown tokens remain source text. If expansion ever exceeds its1KiB
workspace, the full raw text is shown with a visible raw-token notice.

The reader wraps into twelve lines per page, consuming at least one byte per
line, so the1023-byte maximum expanded string fits a hard bound of86 pages.
Every retained character can be reached through paging; long messages are not
silently cut to the first screen. Normal whitespace wrapping follows the
existing HUD behavior. A long level label alone gets an explicit ellipsis;
the full level identity remains stored. The optional original TEXT font has
the existing question-mark fallback for unsupported character codes. Without
usable HUD glyphs, the procedural renderer remains readable and shows a
notice when unsupported bytes are replaced with question marks.

## Pause, rendering and lifetime

`player_message_history_menu` runs inside the suspended `player_pause_menu`
input-provider call. It uses the existing edge-only menu controls,10ms polling
and connection-loss/release guards. Opposite simultaneous directions do not
navigate. Reconnection cannot manufacture A/B/Start or a gameplay action.
The screen redraws only for changed selection, page or connection state.

History never unpauses audio or advances simulation, AI, mission deadlines,
the game-frame counter or wall-clock catch-up. B merely returns to pause;
Start exits through the same audio resume, frame-clock rebase and neutral-input
sample as Resume/Options. Back+Start keeps its established session-exit route.
No input/replay record changes are made.

The draw path never sets `particle_draw_stream`. It borrows the current
`scene_actor_collision_owner->hud_assets` only during a synchronous draw and
uses the existing `scene_hud_message_line` / `scene_hud_text_box` helpers.
`rf_xbox_message_history_draw` always completes `hud_batch_end`, including on
draw error, before returning. That existing fence drains GPU work and clears
its borrowed pixel pointer. Scene retirement cannot run while this menu owns
the suspended input call. A missing or oversized custom font uses the existing
procedural glyph renderer; no new resource admission is attempted.

## Reset and save policy

- Genuine New Game/full-session reset clears history in
  `rf_scene_session_reset`, after the existing idle-owner admission. The
  constructor's `campaign_session_history_reset` also clears on a genuine
  cold start, but explicitly preserves history while quick-load is pending,
  even if the transition flag has already been consumed.
- Confirmed tester End Session clears at `rf_scene_session_end`.
- Accepted fresh-section restart, including death's fresh-section fallback,
  clears at `campaign_section_request_restart`, because it abandons the
  unsaved timeline. A refused restart does not reach that function.
- Ordinary next-section travel/revisit retains the ring; entries own copied
  level and text bytes, so retiring the outgoing level cannot dangle them.
- Save and failed quick-load preflight leave history untouched.
- Successful world restoration clears at the final return boundary of
  `scene_world_load`, guarded by `!status && world_published`. This is after
  native storage close, final owner assignments, rollback decisions and all
  staged owner release; no later fallible operation precedes that return.
  Failed or absent restoration does not clear the prior ring.

The current checkpoint codecs do not supply Message History. Loading a save
therefore starts with empty history; previously received text is not restored
or regenerated by firing events. The UI explicitly says history clears on
restart/load. Existing save-slot selection, compatibility/transient guards,
protected-HDD requirements, session cleanup and load failure policy are intact.

## Parent integration and validation boundary

The existing Xbox objects remain the owners; the two new `.inc` files are
included from `scene.c`, so no Makefile/object addition is needed. Read-only
history declarations are appended to `scene_preview.h` and the platform renderer
header. No proof framework or gameplay fixture accompanies this slice.

Any successor parent controller recipe that reaches Options by wrapping up
from Resume must account for the new History row6 (Options remains5).
The immutable16:00 baseline/helper are unchanged by this staged handoff.

After integration, the scheduled stock64 batch can establish compilation and
linking. A proportional bounded control check can cover opening/empty state,
returning to pause/resuming, and navigating genuinely received text if already
available. Eviction/long-message and reset behavior remain source-reviewed
until tested; no authored event injection or campaign-route traversal is
required or claimed by this handoff. Saved-load checks retain their separate
authorization and protected-data admission requirements.
