# Internal subdivision of the existing renderer draw phase

The16:00 run attributes30.121ms to a broad draw/GPU-wait phase. An additional
8x4word read-only profile records the existing command/fence boundaries so
future optimization can distinguish CPU submission from completion waits.
It adds no fence, reset, command, draw, sleep, input, frame skip or screenshot.

rf_renderer_draw_profile rows are: retained world command submission; CPU mesh
and retained-model submission; optional audit completion; geometry completion
and particle reset; particle/corona commands (including existing bounded
midpass drains); particle completion and HUD reset; HUD commands (including
existing bounded drains); final frame completion. Same calls/low/high/max row
layout and16-frame warmup as the established renderer profile. The ordinary
summary script includes the rows only when captured. These rows are nested in
the broad renderer draw phase, not additional frame cost or an FPS estimate.

Only the parent17:00 existing capture reads the new symbol. No helper test,
new harness or runtime measurement was run.
