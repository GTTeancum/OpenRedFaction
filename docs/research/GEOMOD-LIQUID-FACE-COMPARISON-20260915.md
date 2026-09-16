# Numerical liquid face comparison (2026-09-15)

`tools/verify_liquid_face_collision.py` executes unmodified original4dec10 on30 liquid-quad point/sphere cases and compares the shared PC sweep result. Hit/miss, fraction, surface position and normal match exactly. The freshly linked NXDK function is executed under Unicorn and matches the PC result bytes, including untouched miss outputs and improving-hit count.

Cases include downward/upward travel, starts below/on the plane, parallel travel, overlap, point endpoint and exact-binary sphere endpoint. Decimal0.1/0.9 endpoint behavior retains original floating-point differences. No native original-game launch, screenshot comparison or full room traversal is involved.

The collision world now owns one authored contains-liquid byte per room in its existing allocation, charged to resident/peak budgets. Existing 44082 world sweeps across94levels pass including source-buffer disposal and budget rejection. The general guest hit-record layout remains unchanged. This metadata enables the separate original room-liquid pass; it does not itself enable water gameplay or reproduce dynamic water-face rebuilding.
