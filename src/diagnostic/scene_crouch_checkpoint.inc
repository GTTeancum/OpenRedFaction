#ifndef RF_SCENE_CROUCH_CHECKPOINT_INCLUDED
#define RF_SCENE_CROUCH_CHECKPOINT_INCLUDED
/* Actual physical ownership is independent of current combat/order/weapon.
 * A blocked stand, death pose or ordinary fall must retain the same shape. */
static int scene_crouch_live_shape(uint32_t slot,const campaign_npc_body *o)
{
    const rf_physics_stance_cache *cache;uint32_t cls,physical;int status;
    if(!o||!campaign_npc_bodies||slot>=campaign_npc_body_count||o!=campaign_npc_bodies+slot)return RF_RANGE;
    status=scene_crouch_receipt_admit(&o->crouch);if(status)return status;
    physical=!!(o->view.flags_810&0x400u);
    if(!(o->crouch.flags&SCENE_CROUCH_PRESENT))return physical||o->unholster.stance_7bc?RF_FORMAT:RF_OK;
    if((o->unholster.stance_7bc<0||o->unholster.stance_7bc>1)||(physical&&!(o->crouch.flags&SCENE_CROUCH_STAMP))||
       physical!=!!(o->damage.effects.flags_810&0x400u)||
       scene_npc_reload_identity(slot,o)||!scene_npc_reload_standalone(o))return RF_FORMAT;
    status=scene_tactical_source(slot);if(status)return status;
    cls=campaign_seeds.items[slot].class_index;
    if(!campaign_npc_stances||cls>=campaign_seeds.class_count)return RF_FORMAT;
    cache=campaign_npc_stances+cls;
    if(!cache->count||cache->count>8||cache->count!=o->body.spheres.count||!o->body.spheres.items||
       o->body.spheres.allocated_bytes!=sizeof(o->body.spheres)+(uint64_t)cache->count*sizeof(*o->body.spheres.items)||
       !isfinite(cache->height_difference))return RF_FORMAT;
    for(uint32_t i=0;i<cache->count;i++){
        const rf_physics_sphere *sphere=o->body.spheres.items+i;
        if(!isfinite(sphere->radius)||sphere->radius<0||!isfinite(sphere->parameter_10))return RF_FORMAT;
        for(uint32_t j=0;j<3;j++)if(!isfinite(cache->centers[physical][i][j])||
            sphere->center[j]!=cache->centers[physical][i][j])return RF_FORMAT;
    }
    return RF_OK;
}
/* A residual crouched-fire clip can outlive physical stance or its target.
 * Keep authored mapping/source proof; do not require a currently crouched
 * actor and do not infer completion of any paid gameplay obligation here. */
static uint32_t scene_crouch_action4(uint32_t slot,uint32_t present,
    const rf_entity_motion_selection *selection,int32_t motion)
{
    return present==1&&selection&&motion>=0&&motion==selection->mapping.actions[4]&&
        scene_tactical_source(slot)==RF_OK;
}
/* Bounded future clip identities for a retained stance owner. Authored
 * missing clips remain optional here; live admission independently requires
 * the crouch transition mapping. A stronger AI owner adds its own requests,
 * rather than replacing this set or relying on active-slot references. */
static int scene_crouch_future_clip(const rf_entity_motion_selection *selection,
    uint32_t request,int32_t *id)
{
    static const uint32_t states[4]={0,2,8,9};
    static const uint32_t actions[3]={2,4,39};
    if(!selection||!id||request>=7)return RF_RANGE;
    *id=request<4?selection->mapping.states[states[request]]:
        selection->mapping.actions[actions[request-4]];
    if(*id< -1)return RF_FORMAT;
    return *id<0?RF_NOT_FOUND:RF_OK;
}
static int scene_crouch_capture(uint32_t slot,int32_t now,uint32_t frame,
    rf_npc_checkpoint_record *row)
{
    const campaign_npc_body *o;int status;
    if(!row||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;row->crouch_profile=1;
    if(row->retired){memset(&row->crouch,0,sizeof(row->crouch));return RF_OK;}
    status=scene_crouch_live_shape(slot,o);if(status)return status;
    return scene_crouch_receipt_capture(&o->crouch,o->unholster.stance_7bc,
        !!(o->view.flags_810&0x400u),now,frame,&row->crouch);
}
#endif
