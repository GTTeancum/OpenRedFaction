# Mover_Pause first pass

The three authored L14S2 `Mover_Pause` events now control their linked group
controllers on Xbox. ON sets the controller's pause bit (`0x80`), while OFF
clears it. Ordinary OFF link propagation then stops a linked mover through
`rf_group_motion_stop`; this also applies to other event types with OFF links.

The installed PC executable provides the behavior evidence without a live PC
reference run: event factory type 72 at `0x4b7451`, ON method `0x4bd0f0`, OFF
method `0x4bd140`, controller flag setters `0x46b930` and `0x46b950`, and
generic OFF mover dispatch `0x4b6640` to stop method `0x46b5b0`. This is a
functional reconstruction, not a byte-for-byte port.

`python tools/xemu_mover_pause.py` stages authored L14S2 event 9879 on the
`Tram 01` controller, then event 10201 (`Invert`) at frame 60 to send 9879
OFF. The stock-64-MiB Xbox guest completed 90 frames. A live frame-37 probe
recorded one ON, pause bit set, no OFF or stop; the final state recorded one
OFF, one stop, pause bit clear, and no next key. The guest reported 4,610 free
pages. The focused run was
`artifacts/xemu/mover-pause-20260930-022830`; no PC gameplay run was used.

The setup fixture forces event timing. Natural L14S2 triggers, motion as
actually rendered, simultaneous mover interactions and active-mover
save/reload remain open.
