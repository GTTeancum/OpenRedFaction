/* AI14 gameplay and model action43 have separate lifetimes. No transient
 * registry identity/timer is serialized in the finite RFNC/RFCH lane. */
typedef struct scene_medic_action {
    uint32_t pending,clip_owned,source,target;
    const rf_entity_view *source_view,*target_view;
    int32_t deadline,motion,default_action;
} scene_medic_action;
