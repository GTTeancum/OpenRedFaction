# Blind-pursuit expiry follow-up

Separate from the frozen13:00 input d930ad5. No build or runtime verification.
Original RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

-41c4ff..41c510 reads required $Blind Pursuit Time into class718.
-4022d8..4022f3 updates last-seen298 only when either visibility byte is1.
-405d71..405db5 and406dbf..406e06 drop the target strictly when game seconds
 minus last-seen seconds exceeds class718, except in either network mode.

Added the bounded class metadata reader/loader and exact finite-snapshot
expiration predicate, then connected the default vehicle combat owner to them.
Sub's table20 seconds and both Fighter classes'50 seconds are loaded rather
than hardcoded. Loss of covered sight advances retained elapsed time; renewed
sight resets it. Expiry clears the weak target and guidance while emitted
rounds continue to fly with their original source identity.

RFAI2 reuses row60 for elapsed unseen seconds, keeping the row64/flight52
shape. Existing RFAI1 loads start a fresh class grace, because they did not
store sight age. No timestamp from a previous process is restored. The scene
still uses its documented practical every-frame sight service and direct
steering, not retail randomized200–399ms dual-ray perception or tactical AI.
