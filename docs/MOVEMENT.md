# Movement settings used by turn animations

`rf_movement_set_mode` in src/core/movement.c reconstructs complete routine
0x427450 and predicate 0x40a210. It returns entity settings as explicit shared
C state rather than writing a raw original entity structure. This is the
routine called by candidate/turn helper 0x41f9f0 with requests zero or one.

Inputs map as follows:

| Shared input | Original source |
| --- | --- |
| forced_action | Entity +75c |
| entity_scale | Entity +98 |
| config.flags | Entity info (+294 pointer), offset +724 |
| config.base_speed | Info +50 |
| config.slow_factor | Info +54 |
| config.alternate_factor | Info +58 |
| config.response | Info +5c |
| override_enabled | Global byte 0x64ecb9 |
| config.override_slow | Global float 0x594590, initial value 7 |
| config.override_normal | Global float 0x59458c, initial value 9 |

A forced_action value other than -1 replaces the request with zero. Request
zero sets entity +8c4 to zero and +8c0 to slow_factor times base_speed, or the
slow override when the global byte is nonzero. Request two sets mode two and
speed to alternate_factor times base_speed; it ignores the global override.
All other requests, including negative values, set mode one and speed to
base_speed or the normal override. The API supplies override values explicitly
because the original reads mutable globals rather than fixed constants.

When config.flags bit 0x800 is set, the routine also updates entity +8c.
Request zero sets it to 3000. Other requests set it to config.response times
entity_scale divided by base_speed, without intermediate float stores. When
that flag is clear, +8c remains unchanged. The field is called `response` only
as a neutral API label; its complete physical interpretation remains unresolved.
The output speed and mode feed later animation selection through +8c0/+8c4.

The C API validates finite configuration, rejects a zero denominator when
needed, and rejects non-finite calculated outputs without partial mutation.
It allocates nothing. Both PC and NXDK build the same implementation.

`tools/verify_movement.py` compares 6,000 original executions with the unmodified
predicate against C. It checks exact bytes of all three output fields and that
no other original entity RAM changed. Cases vary forced actions, all request
classes, descriptor flags, scales and mutable override values. Three C-only
rejection cases check unchanged state. Evidence:
`artifacts/movement-settings-verification.json`.

These settings do not implement movement integration, collision or player input.
The next integration point is the turn helper's combination of action starts,
five deadlines, entity +7bc and this settings routine.
