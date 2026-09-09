#ifndef RF_ENTITY_H
#define RF_ENTITY_H
#include <stdint.h>
#include "rf/vpp.h"

#define RF_OBJECT_SLOTS 1024
/* Object flags assembled by 422360 before generic allocation. descriptor_kind
 * is class descriptor +0x94. Remaining factory initialization is separate. */
uint32_t rf_entity_creation_object_flags(uint32_t creation_flags,uint32_t descriptor_kind);
/* Compact caller-owned views of the fields required by entity predicates.
 * These are not binary RF.exe structs or complete gameplay entities. */
typedef struct rf_entity_view {
    int32_t handle, type, class_type;
    uint32_t flags_7c, flags_810, flags_7d0;
    int32_t action_520, linked_handle, weapons[2];
    float base_speed;
    const struct rf_entity_view *weapon_owner;
    const int32_t *occupants;
    uint32_t occupant_count;
} rf_entity_view;
typedef struct rf_entity_registry {
    const rf_entity_view *slots[RF_OBJECT_SLOTS];
} rf_entity_registry;

/* 0x40a0e0 / 0x426fc0: low 16 bits index, full handle comparison, then type 0. */
const rf_entity_view *rf_object_lookup(const rf_entity_registry *registry, int32_t handle);
const rf_entity_view *rf_entity_lookup(const rf_entity_registry *registry, int32_t handle);
/* 0x408dc0: local weapon or recursive first occupied seat of a zero-speed
 * owner. Input views must remain stable during the call. Cycles return
 * RF_FORMAT rather than reproducing unbounded original recursion. */
int rf_entity_has_weapon(const rf_entity_registry *registry, const rf_entity_view *entity, int *result);
/* 0x41f950 and the complete combat gate at 0x41f678..0x41f69c. attached is
 * a stable, ordered snapshot of handles from the original 0x7c75cc list.
 * Output unchanged on malformed views. No allocations or entity mutations. */
int rf_entity_combat_predicates(const rf_entity_registry *registry, const rf_entity_view *entity,
    const int32_t *attached, uint32_t attached_count, int *ready, int *eligible);
#endif
