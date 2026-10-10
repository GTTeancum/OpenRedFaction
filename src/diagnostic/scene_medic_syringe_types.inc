#ifndef RF_SCENE_MEDIC_SYRINGE_TYPES_INCLUDED
#define RF_SCENE_MEDIC_SYRINGE_TYPES_INCLUDED
/* The sole generated child role is medic hand1 syringe. No authored UID is
 * allocated for this relationship. Registry generation belongs only to the
 * live type4 child; checkpoints key the separate lane by its parent UID. */
typedef struct scene_medic_syringe {
    rf_clutter_base_owner *child;scene_clutter_damage_binding damage;
    const rf_registered_entity_view *parent;
    uint32_t parent_handle,parent_slot,state,unavailable;
    float health;int32_t hand_tag,fault;
} scene_medic_syringe;
typedef struct scene_medic_syringe_candidate {
    rf_clutter_base_owner *child;scene_clutter_damage_binding damage;
    const rf_registered_entity_view *parent;
    uint32_t parent_handle,parent_slot,state,prepared,retired;
    float health;int32_t hand_tag;
    uint32_t allocated_bytes,peak_bytes;
} scene_medic_syringe_candidate;
#endif
