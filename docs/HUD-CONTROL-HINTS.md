# Authored control hints and interaction-cue assessment

Source-written2026-10-08 for the parent's22:00 batch. No helper builds, tests,
screenshots, game input or campaign traversal. This follows the detonator
display correction and does not add a new interaction feature.

## Concrete original message evidence

Read-only inspection of installed level-local English message tables found25
action-token occurrences, covering11 distinct names in train01_text.tbl and
train02_text.tbl. Authored instructions explain operating buttons/monitors,
entering and leaving turrets/submarines, talking to people, movement and weapon
controls. They use placeholders such as $USE$, not a hard-coded PC key.

rf_level_message_parse intentionally retains those bytes in the bounded
message owner. The former HUD rendered them literally, leaving instructions
such as Use ($USE$) unclear on an Xbox controller. This is a concrete existing
instruction-display gap, independent of optional target-hover UI.

## Source-confirmed bindings

The renderer now expands the authored presentation string according to the
current fixed frontend bindings in src/platform/xbox/input.c:

- $USE$ -> X
- $FIRE$ -> RT
- $ALT_FIRE$ -> LT
- $JUMP$ -> A
- $CROUCH$ -> B
- $RELOAD$ -> Y
- $NEXT_WEAPON$ -> D-PAD RIGHT
- $PREV_WEAPON$ -> D-PAD LEFT

These eight mapped names cover22 observed occurrences. The remaining three
original actions, $HOLSTER$, $MESSAGE_LOG$ and $FINE_AIM$, have no corresponding
input field/binding in this first-playable frontend and display UNBOUND.
No nonexistent controller button or unsupported feature is promised. Unknown
tokens are preserved literally. The PC staging branch uses its existing
keyboard/mouse defaults from play.c; no PC runtime work was performed.

The labels are descriptions of current fixed controls, not a new input map.
If input remapping is later implemented, the descriptions must be supplied by
that binding owner instead of this explicit current-default table.

## Lifetime and bounds

Expansion occurs only after the ordinary message-expiry check, into a1KiB
call-local buffer. It does not change campaign_subtitle, its UID, voice owner,
the latest-message-wins rule, event timing, or the original-length-based reading
deadline. Both the original-font and procedural fallback paths consume the
same expanded bytes synchronously. Wrapping is given the actual presentation
buffer capacity, rather than silently retaining the old512-byte assumption.

Reads stay inside the512-byte source owner; output appends reserve a terminator
and reject overflow. Unknown dollar text advances without looping. Successful
output is explicitly terminated; on expansion failure the caller keeps the
original message and records the failure. There is no allocation, archive read,
input polling, token evaluation or gameplay callback. Common plain characters
copy directly instead of issuing one freestanding memcpy call per character.

rf_hud_control_text_diagnostic[8] records expansion calls, mapped tokens,
explicitly-unbound tokens, unknown tokens, failures, last output byte count,
message UID and last status. It resets with normal HUD admission. Counters are
per presented message frame; positive mapped counts are not expected from a
neutral L1S1 line that contains no action tokens.

## Why no generic hover prompt was added

The inspected hud.tbl has corpse-specific rows40/41, but no general Use/Enter
cue. ui.vpp has no dedicated interaction indicator among its inspected HUD
assets, and strings.tbl #147 is the Use control name beside Fire/Alt Fire/Jump,
not a localized generic target prompt. This limited source inspection does
not prove that every original HUD code path lacks such a cue. It establishes
the authored training-instruction path without justifying a new hover feature.

Existing turret selection runs on an actual Use edge and checks ownership,
range, facing and cover. Vehicle entry/promotion includes staged dependency,
seat, motion and clearance admission. Trigger contact polling owns dwell and
activation state, with extended reach checks when Use is actually pressed.
None should be called as a mutating render probe, and proximity alone cannot
honestly promise success. A general read-only interaction-candidate service is
therefore deferred unless a concrete playability need or recovered original
cue requires it. The current actual Use binding remains X.

No new per-slice fixture or test was created. Parent22:00 compilation/runtime
remains pending; the original-data inventory above is source inspection,
not execution of the new expansion code.
