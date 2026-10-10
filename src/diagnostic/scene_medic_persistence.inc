#ifndef RF_SCENE_MEDIC_PERSISTENCE_INCLUDED
#define RF_SCENE_MEDIC_PERSISTENCE_INCLUDED
/* Finite state only. Transient action14/clip43 belongs to scene_medic_use.
 * The dedicated child role is constructor hand1/syringe, not an authored UID.
 * Include after campaign_npc_body and the syringe API declarations. */
static int scene_medic_state_admit(const campaign_npc_body *o,const rf_campaign_medic_state *m)
{
    int status=rf_campaign_medic_state_validate(m);if(status)return status;
    if(!o)return RF_RANGE;
    if((m->reserve_seen||m->child_state)&&!scene_medic_syringe_qualified(o))return RF_FORMAT;
    return RF_OK;
}
static int scene_medic_state_capture(const campaign_npc_body *o,rf_campaign_medic_state *out)
{
    rf_campaign_medic_state m={0};int status;
    if(!o||!out)return RF_RANGE;
    if(!scene_medic_syringe_qualified(o)){
        if(o->medic_reserve_seen||o->medic_reserve!=0)return RF_FORMAT;
        *out=m;return RF_OK;
    }
    /* Untouched runtime reserve is200, but absent wire/history is all zero.
     * An exhausted seen reserve is copied exactly, never derived by subtraction. */
    if(o->medic_reserve_seen>1||!isfinite(o->medic_reserve)||
       o->medic_reserve<0||o->medic_reserve>200.f||
       (!o->medic_reserve_seen&&o->medic_reserve!=200.f))return RF_FORMAT;
    m.reserve_seen=o->medic_reserve_seen;
    if(m.reserve_seen)m.reserve=o->medic_reserve;
    status=scene_medic_syringe_capture(o,&m.child_state,&m.child_health);if(status)return status;
    status=rf_campaign_medic_state_validate(&m);if(status)return status;
    *out=m;return RF_OK;
}
/* Assignment-only finite lane. Same-level restore invokes this only after
 * final storage success and after campaign history has rebound actor keys. */
static void scene_medic_state_assign(campaign_npc_body *o,const rf_campaign_medic_state *m)
{
    o->medic_reserve_seen=m->reserve_seen;
    o->medic_reserve=scene_medic_syringe_qualified(o)?(m->reserve_seen?m->reserve:200.f):0.f;
}
/* Section revisit constructs from the saved level/UID lane before publication.
 * Retirement suppresses actual child resources while retaining role metadata.
 * No reserve/child mutation occurs if candidate construction fails. */
static int scene_medic_history_restore(campaign_npc_body *o,const rf_campaign_medic_state *m,uint32_t retired)
{
    scene_medic_syringe_candidate candidate={0};int status;
    if(retired>1)return RF_FORMAT;
    status=scene_medic_state_admit(o,m);if(status)return status;
    if(scene_medic_syringe_qualified(o)){
        status=scene_medic_syringe_restore_prepare(o,m->child_state,m->child_health,retired,64u*1024u,&candidate);
        if(!status)status=scene_medic_syringe_restore_validate(o,&candidate);
        if(status){scene_medic_syringe_restore_discard(&candidate);return status;}
        scene_medic_syringe_restore_publish(o,&candidate);
        scene_medic_syringe_restore_discard(&candidate);
    }
    scene_medic_state_assign(o,m);return RF_OK;
}
#endif
