#ifndef RF_SCENE_NPC_CHECKPOINT_CAPTURE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_CAPTURE_INCLUDED
#include "rf/npc_checkpoint.h"
/* Include after campaign NPC/seed/pose/persistence declarations. Caller owns
 * stable scene state throughout both passes. Global projectile, shield, burn,
 * mover, event and pickup admission remains the composed caller's duty. */
static int scene_npc_checkpoint_row(uint32_t index,int32_t now,rf_npc_checkpoint_record *row)
{
    const campaign_npc_body *o=campaign_npc_bodies+index;
    const rf_campaign_object_record *key;const rf_entity_pose *pose;
    uint32_t i,slot=o->persistence_slot;int pending,status;
    int32_t deadlines[5]={o->pain.ai_timer,o->pain.animation_lock,o->pain.cooldown,
        o->unholster.deadline_518,o->death.deadline_4b8};
    if(!o->persistence_registered){return o->registration.view?RF_FORMAT:RF_NOT_FOUND;}
    if(slot>=rf_scene_defeated_actors.count||slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
    key=rf_scene_defeated_actors.items+slot;
    if(key->level>=rf_scene_defeated_actors.level_count||
       strcmp(rf_scene_defeated_actors.levels[key->level],campaign_current_level)||
       key->uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
       campaign_seeds.items[index].class_index>=campaign_seeds.class_count)return RF_FORMAT;
    if(!o->registration.view&&!key->retired)return RF_FORMAT;
    if(o->registration.view&&(o->registration.view!=&o->view||
       rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)!=&o->view))return RF_FORMAT;
    /* Never silently discard a route, including its inactive bound path. */
    if(o->script_move.active||o->script_move.path.count||o->navigation.retained.count||
       o->script_animation.active||o->script_animation.freeze||o->view.linked_handle!=-1||
       o->view.occupant_count||o->attachment_75c!=-1||o->controller_handle||
       o->combat_alert||o->combat_scripted||o->combat_target||o->combat_burst_remaining||
       o->combat_due||o->combat_reload_due||o->combat_navigation_due||o->damage.effects.burn)return RF_RANGE;
    for(i=0;i<64;i++)if(o->firing.active[i])return RF_RANGE;
    if(o->registration.view){
        for(i=0;i<5;i++){status=rf_timer_pending(deadlines[i],now,&pending);if(status)return status;if(pending)return RF_RANGE;}
        if(index>=campaign_poses.count||!campaign_poses.items)return RF_FORMAT;pose=campaign_poses.items+index;
        if(pose->playback.completion.active.count>16||pose->playback.completion.frozen)return RF_RANGE;
        /* Only standing idle loops may restart their cosmetic phase. Any
         * one-shot, attack, death or locomotion clip requires a future payload. */
        for(i=0;i<pose->playback.completion.active.count;i++){
            int32_t motion=pose->playback.completion.active.slots[i].motion;
            if(key->retired||motion!=o->selection.mapping.states[0]||motion<0||
               !campaign_playback_resources.models||pose->skeleton>=campaign_playback_resources.model_count||
               (uint32_t)motion>=campaign_playback_resources.models[pose->skeleton].count||
               !campaign_playback_resources.models[pose->skeleton].resources||
               !campaign_playback_resources.models[pose->skeleton].resources[motion].looping)return RF_RANGE;
        }
        if(!o->body.allocated_bytes||!o->body.spheres.count)return RF_RANGE;
        for(i=0;i<3;i++)if(!isfinite(o->body.state.velocity[i])||fabsf(o->body.state.velocity[i])>.001f||
           o->command_714[i]!=0||o->look.command_708[i]!=0||o->support_velocity[i]!=0||
           o->look.angles.delta_888[i]!=0||o->look.angles.offset_894[i]!=0||o->look.angles.vector_870[i]!=0||
           o->look.angles.angles_87c[i]!=0)return RF_RANGE;
    }
    if(o->look.body_angles[0]!=0||o->look.body_angles[2]!=0)return RF_RANGE;
    memset(row,0,sizeof(*row));row->uid=key->uid;row->class_id=campaign_seeds.items[index].class_index;
    row->retired=key->retired;row->flags=o->object_flags&0x4004u;row->affiliation=o->damage.effects.affiliation;
    row->health=o->damage.effects.health;row->armor=o->damage.effects.armor;
    memcpy(row->position,o->registration.view?o->body.state.position:o->published,12);
    row->yaw=o->look.body_angles[1];row->primary=o->view.weapons[0];row->secondary=o->view.weapons[1];
    row->drop=rf_scene_defeated_actors.drops[slot];row->inventory=o->inventory;row->ai_mode=o->ai_mode.action_280;
    return RF_OK;
}
static int scene_npc_checkpoint_row_compare(const void *a,const void *b)
{uint32_t x=((const rf_npc_checkpoint_record*)a)->uid,y=((const rf_npc_checkpoint_record*)b)->uid;return x<y?-1:x>y;}
static int scene_npc_checkpoint_capture(const rf_npc_checkpoint_catalog *catalog,int32_t now,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{
    rf_npc_checkpoint_record row;unsigned char encoded[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW],identity[32]={0};
    uint32_t i,j,n=0,written;int status;
    if(!catalog||!count||now<0||now>RF_TIMER_PERIOD||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.items||!campaign_seeds.records.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    status=rf_npc_checkpoint_encode(identity,catalog,NULL,0,encoded,sizeof(encoded),&written);if(status)return status;
    for(i=0;i<campaign_npc_body_count;i++){
        status=scene_npc_checkpoint_row(i,now,&row);if(status==RF_NOT_FOUND)continue;if(status)return status;
        status=rf_npc_checkpoint_encode(identity,catalog,&row,1,encoded,sizeof(encoded),&written);if(status)return status;
        for(j=0;j<i;j++)if(campaign_npc_bodies[j].persistence_registered&&
            (uint32_t)campaign_seeds.records.items[j].record.uid==row.uid)return RF_FORMAT;
        n++;
    }
    if(n>capacity||(n&&!rows))return RF_RANGE;
    n=0;for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].persistence_registered){
        /* No callbacks/mutations occurred since preflight; this pass cannot fail. */
        (void)scene_npc_checkpoint_row(i,now,rows+n);n++;
    }
    if(n)qsort(rows,n,sizeof(*rows),scene_npc_checkpoint_row_compare);*count=n;return RF_OK;
}
#endif
