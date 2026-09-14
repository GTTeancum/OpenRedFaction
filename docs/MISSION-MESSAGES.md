# Mission dialogue integration

Read-only inventory `tools/inspect_mission_messages.py` identifies level-local
`<level>_text.tbl` files alongside the RFLs. Records pair an integer message ID
with a quoted WAV filename, followed by En/Gr/Fr quoted subtitle fields.
These are separate from the global UI `strings.tbl`. Message event words[0]
references the level-local message ID.

The installed inventory contains66 tables and918 entries. All714 authored
Message events resolve; no event-bearing level lacks a text table. The largest
table is11488 bytes and longest English field is182 bytes in this installation.
These observed sizes support a small bounded reader; limits still need explicit
overflow/error handling rather than assuming future inputs have the same size.

Six voice references are absent from the indexed archives: L20S2 message11,
and L8S2 messages2/9/12/14/15. Preserve subtitle display when a voice is absent.
Source hashes, per-level counts and missing references are recorded locally in
`artifacts/mission-messages.json`; original dialogue text remains untracked.

Shared bounded lookup is implemented in `rf_level_message_parse/read`. It
reads from the RFL archive using the entry filename, caps temporary table input
at64KiB, copies the selected record into a580-byte result, and releases the
table allocation before returning. Failed reads preserve the caller output.
English is the first-pass display language; voice filenames are retained.

`tools/verify_mission_messages.py` independently compares all918 installed
English records across66 tables with the C parser. All match. The37-test PC
suite passes, including malformed/truncated, oversized, duplicate requested ID
and missing-ID cases. PC and NXDK Xbox builds succeed. This verifies lookup,
not live presentation or native runtime behavior.

Message event type15 now dispatches through a borrowed `show_message` callback
with the authored record, event clock and on/off state. Speaker links do not
propagate gameplay activation. Delayed messages use the same callback when due;
missing resources are reported without aborting the event chain, while other
backend errors propagate. This is a practical first-pass presentation contract,
not a claim of original queue or interruption behavior.

The contained L1S1 event8356 test reads its real text and verifies delivery,
delay, off requests, missing/error results and speaker-link isolation.

The scene now registers a bounded subtitle owner. English text wraps at46
characters into up to five lines above the combat HUD; lowercase is displayed
in the existing bitmap font as uppercase. First-pass reading time is55ms per
character, clamped to4-12 seconds. The latest valid message replaces the current
one; off requests clear only the matching event. Missing messages preserve the
current line, and scene loading clears it. Voice playback remains open.

PC process-local event8356 captures at120 and600 frames were individually
inspected: the guard line appears, then expires. Local evidence lives in
`artifacts/subtitles/message.png` and `expired.png`. All37 tests and both PC
and NXDK builds pass. Native XEMU subtitle validation remains open.

Remaining: native validation, voice ownership/playback, longer-text presentation
coverage, full character/language support, authored queue and interruption
behavior, speaker placement and timing. The current display is a playable
first pass, not original presentation parity.
