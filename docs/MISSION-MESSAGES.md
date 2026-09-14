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

Next implementation: shared bounded table lookup, Message event dispatch, a
timed subtitle display, and voice playback through the existing audio owner.
Queue/interrupt behavior, speaker placement, timing and language handling need
explicit first-pass decisions and validation. No live message support is claimed
by the inventory alone.
