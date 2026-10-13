#ifndef RF_SCENE_CROUCH_CHECKPOINT_SELECTOR_INCLUDED
#define RF_SCENE_CROUCH_CHECKPOINT_SELECTOR_INCLUDED
/* Two-sided lane selection before any motion demand. This is admission,
 * never capture-time initialization, standing, expiry or shape repair. */
static int scene_crouch_live_present(uint32_t *present)
{
    uint32_t found=0;int status;
    if(!present||(campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    for(uint32_t slot=0;slot<campaign_npc_body_count;slot++){
        const campaign_npc_body *o=campaign_npc_bodies+slot;
        if(!o->crouch.flags&&!o->unholster.stance_7bc&&!(o->view.flags_810&0x400u))continue;
        status=scene_crouch_receipt_admit(&o->crouch);if(status)return status;
        if(!(o->crouch.flags&SCENE_CROUCH_PRESENT)||(o->unholster.stance_7bc<0||o->unholster.stance_7bc>1)||
           ((o->view.flags_810&0x400u)&&!(o->crouch.flags&SCENE_CROUCH_STAMP))||
           scene_npc_reload_identity(slot,o)||!scene_npc_reload_standalone(o))return RF_FORMAT;
        status=scene_tactical_source(slot);if(status)return status;
        found=1;
    }
    *present=found;return RF_OK;
}
static int scene_crouch_incoming_present(const rf_npc_checkpoint_record *rows,uint32_t count,
    uint32_t *present)
{
    uint32_t found=0;int status;
    if(!present||(count&&!rows))return RF_RANGE;
    for(uint32_t i=0;i<count;i++){
        const rf_npc_checkpoint_record *r=rows+i;scene_crouch_receipt prepared;
        uint32_t slot,intent,physical;
        status=scene_crouch_receipt_prepare(&r->crouch,r->crouch_profile,0,0,
            &prepared,&intent,&physical);if(status)return status;
        if(!r->crouch.flags)continue;
        if(r->retired||!campaign_seeds.records.items||!campaign_seeds.items||
           !campaign_npc_bodies||campaign_seeds.records.count<campaign_npc_body_count)return RF_FORMAT;
        for(slot=0;slot<campaign_npc_body_count;slot++)
            if((uint32_t)campaign_seeds.records.items[slot].record.uid==r->uid)break;
        if(slot==campaign_npc_body_count||campaign_seeds.items[slot].class_index!=r->class_id||
           scene_npc_reload_identity(slot,campaign_npc_bodies+slot))return RF_FORMAT;
        status=scene_tactical_source(slot);if(status)return status;
        /* Incoming seat/player ownership is checked by the composed world
         * joins; do not infer the incoming state from its destination alone. */
        found=1;
    }
    *present=found;return RF_OK;
}
#endif
