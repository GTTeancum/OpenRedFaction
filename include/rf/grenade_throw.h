#ifndef RF_GRENADE_THROW_H
#define RF_GRENADE_THROW_H
#include <stdint.h>
#include "rf/weapon.h"
#define RF_GRENADE_THROW_PRIMARY 1u
#define RF_GRENADE_THROW_ALTERNATE 2u
#define RF_GRENADE_THROW_START 1u
#define RF_GRENADE_THROW_RELEASE 2u
/* Port fixed60Hz release scheduler: primary102 ticks (1.7s), alternate96
 * (1.6s). Caller starts the matching animation on START. One successful release
 * starts180 cooldown ticks (3s); this scheduling choice is practical port policy.
 * Zero initialize. phase0 idle,1 windup,2 release awaiting synchronous resolve.
 * Inventory remains externally owned, never copied into this controller. */
typedef struct rf_grenade_throw {
    uint32_t phase,ticks,cooldown,held,alternate;
} rf_grenade_throw;
typedef struct rf_grenade_throw_event {uint32_t kind,alternate;} rf_grenade_throw_event;
/* New primary/alternate edge starts when selected, reserve>0 and off cooldown.
 * Alternate wins simultaneous edges. Letting go does not cancel an accepted
 * throw. Selection loss cancels windup; cooldown continues while unselected.
 * Call once per simulation tick even off-weapon to maintain cooldown and edges.
 * RELEASE is emitted once, then step waits for resolve without duplicate events.
 * Recheck reserve immediately before spawning, then resolve in the same tick.
 * If another system consumed the final grenade during windup, cancel on release
 * without spawning. Invalid inputs preserve state/event. No allocation. */
int rf_grenade_throw_step(rf_grenade_throw *,uint32_t trigger_bits,
    uint32_t selected,int32_t reserve,rf_grenade_throw_event *);
/* After RELEASE: spawned0 cancels without ammo debit/cooldown and requires a
 * fresh edge for retry. spawned1 requires reserve>0, decrements exactly once,
 * and starts cooldown. Calling twice is an error. On failure neither changes.
 * Resolve before any inventory mutation or another controller step; a successful
 * game projectile spawn and this acknowledgement form one caller transaction. */
int rf_grenade_throw_resolve(rf_grenade_throw *,uint32_t spawned,int32_t *reserve);
#endif
