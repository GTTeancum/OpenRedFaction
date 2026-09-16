#ifndef RF_CLUTTER_DAMAGE_H
#define RF_CLUTTER_DAMAGE_H
#include "rf/clutter_gameplay.h"
typedef struct rf_clutter_damage_state {
    float health;
    uint32_t object_flags;
    int32_t killing_type;
} rf_clutter_damage_state;
typedef struct rf_clutter_damage_result {
    rf_clutter_damage_state state;
    uint32_t admitted,health_break_due,retirement_marked;
} rf_clutter_damage_result;
/* Pure ordinary-prop410270 arithmetic AFTER admission: type-1 bypasses scaling,
 * types0..10 use descriptor factors. No health clamp. Killing type changes only
 * when the arithmetic result is <=0. Caller excludes riot shield special class.
 * Signed finite damage/factors are valid here. No armor/flags/effect mutation.
 * Result flags independently expose stored health<=0 and existing genericbit2;
 * neither result grants a one-shot break/effect or ownership-release operation.
 * Existing marked props can still be health-dead; positive-health GeoMod
 * retirement alone never becomes health_break_due. Caller owns update timing,
 * corpse/glass policy, shared class cooldown and safe deferred retirement.
 * No allocation; source and output may alias through result.state. All errors
 * preserve output. Invalid type/nonfinite input or unrepresentable output is
 * RF_RANGE. Only the selected factor must be finite. */
int rf_clutter_damage_apply(const rf_clutter_gameplay_definition *,
    const rf_clutter_damage_state *,float damage,int32_t damage_type,
    rf_clutter_damage_result *);
/* Ordinary registered SP clutter admission subset4892c0: damage<.001f is
 * ignored; otherwise mark generic damagedbit0x200000 BEFORE checking protected
 * bit4. Nonzero override bypasses bit4. Rejection returns RF_OK/admitted0 and
 * retains health/killing type, including the damaged flag side effect above.
 * Caller must resolve a valid clutter handle and exclude actor/network/shield
 * special paths before using this helper. It does not perform blast falloff,
 * hit tests, target generation lookup or lifecycle dispatch. */
int rf_clutter_damage_receive(const rf_clutter_gameplay_definition *,
    const rf_clutter_damage_state *,float damage,int32_t damage_type,
    uint32_t override_protection,rf_clutter_damage_result *);
#endif
