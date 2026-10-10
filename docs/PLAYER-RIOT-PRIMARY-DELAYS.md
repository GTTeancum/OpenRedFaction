# Player Riot primary authored contacts

## Source evidence and implementation

RF.exe 4a579b–4a57f8 arms actor timers +4c0/+4c4 from primary descriptor
+150/+154. Original Riot Stick metadata supplies 0.15 and 0.6 seconds.
Player update 4a281b–4a2848 consumes all overdue timers into one Boolean;
4a28fa dispatches the contact factory once after that loop. NPC 409340 differs:
its factory call 40956b is inside the timer loop. The two adapters deliberately
retain this distinction.

An accepted ordinary Riot primary now owns two independently scheduled contact
deadlines. Each actual service opportunity uses the current eye and forward
orientation. Two deadlines overdue on the same simulation frame produce one
contact attempt, without catch-up attacks. The normal trigger remains the only
cadence/admission owner; primary costs no battery. Attack presentation/onset is
published once when admitted, and damage is deferred to the authored contacts.
Primary release does not discard an already accepted swing.

The contact adapter retains the existing 2.6-unit primary ray and immutable
primary damage profile, including 60 base damage per actual contact. Restoring
the second authored contact can increase total damage compared with the old
single-contact port. No compensating damage reduction is invented. Current
world/mover, rubble, retained-prop, vehicle, NPC physical-shield and body damage
consumers remain in the contact path. This is the existing practical ray policy,
not a claim of original swept-collider parity.

## Lifetime and interruption

One full player handle and monotonic ticket own the swing. Reentrant presentation
or contact reservations survive cancellation/reset until their stack unwinds.
Due bits retire before callbacks; callbacks cannot resurrect the second contact.
Source identity and target full handles are checked again around scene callbacks.

Accepted reload, charged alternate takeover, live weapon/form changes, inventory
strip, death, vehicle/turret boarding, actual teleport and timeline replacement
retire the pending primary. A reload key that cannot reload does not cancel it.
An empty alternate with no reserve cannot postpone its contacts. Charged alternate
cancellation remains explicit port policy. The separately integrated alternate
owner now uses one accepted10ms contact with primary60/bash instead of the
former held120-per-second electrical policy; it cancels only this primary owner
on charged takeover. This primary timing source and its release behavior are
unchanged. Existing finite fractional charge drain remains an explicit alternate
approximation; see `PLAYER-RIOT-ALTERNATE-PULSES.md`.

No new save format is introduced. Live save capture rejects a pending primary
or callback reservation, including seated capture. Load preflight is not gated
by that outgoing transient. RFCP success and ordinary world publication plus
successful storage close retire it; shared selector/relocation helpers used by
load preparation do not cancel it unconditionally.

## Verification boundary

Source integration and independent timer/contact/lifecycle reviews completed after
the failed 03:00 UTC build. No compilation,
action runtime, native audio output, save restoration or original visual parity
is established by these changes. The next consolidated hourly batch is the first
allowed compilation. A neutral startup check can admit metadata/resources only;
it cannot establish either Riot contact or interruption behavior.
