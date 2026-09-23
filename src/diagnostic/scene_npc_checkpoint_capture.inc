#ifndef RF_SCENE_NPC_CHECKPOINT_CAPTURE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_CAPTURE_INCLUDED
#include "rf/npc_checkpoint.h"
/* Include after campaign NPC/seed/pose/persistence declarations. Caller owns
 * stable scene state throughout both passes. Global projectile, shield, burn,
 * mover, event and pickup admission remains the composed caller's duty. */
/* Persistence canonicalizes ASCII names, while the loaded archive member may
 * retain authored capitalization. Compare bounded names without mutating either. */
static int scene_npc_checkpoint_level_equal(const char *a,const char *b)
{
    uint32_t i;
    for(i=0;i<64;i++){
        unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A'&&x<='Z')x=(unsigned char)(x+32);
        if(y>='A'&&y<='Z')y=(unsigned char)(y+32);
        if(x!=y)return 0;
        if(!x)return i!=0;
    }
    return 0;
}
static int scene_npc_checkpoint_reject(uint32_t index,const char *reason,uint32_t part,double value,int status)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *enabled=getenv("RF_REPLAY_WORLD_CHECKPOINT_PROBE");
    if(enabled&&!strcmp(enabled,"1"))printf("WORLD_CHECKPOINT_NPC_ADMISSION uid%u reason%s part%u value%.9g status%d\n",
        (uint32_t)campaign_seeds.records.items[index].record.uid,reason,part,value,status);
#else
    (void)index;(void)reason;(void)part;(void)value;
#endif
    return status;
}
static int scene_npc_checkpoint_row_mode(uint32_t index,int32_t now,rf_npc_checkpoint_record *row,uint32_t initializing)
{
    const campaign_npc_body *o=campaign_npc_bodies+index;
    const rf_campaign_object_record *key;const rf_entity_pose *pose;
    uint32_t i,slot=o->persistence_slot;int pending,status;
    int32_t deadlines[5]={o->pain.ai_timer,o->pain.animation_lock,o->pain.cooldown,
        o->unholster.deadline_518,o->death.deadline_4b8};
    if(!o->persistence_registered){return o->registration.view?RF_FORMAT:RF_NOT_FOUND;}
    if(slot>=rf_scene_defeated_actors.count||slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
    key=rf_scene_defeated_actors.items+slot;
    if(key->level>=rf_scene_defeated_actors.level_count||key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
       !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level)||
       key->uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
       campaign_seeds.items[index].class_index>=campaign_seeds.class_count)return RF_FORMAT;
    if(!o->registration.view&&!key->retired)return RF_FORMAT;
    if(o->registration.view&&(o->registration.view!=&o->view||
       rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)!=&o->view))return RF_FORMAT;
    /* Never silently discard a route, including its inactive bound path. */
#define NPC_CAPTURE_ZERO(field) do{if(o->field)return scene_npc_checkpoint_reject(index,#field,0,(double)o->field,RF_RANGE);}while(0)
    NPC_CAPTURE_ZERO(script_move.active);NPC_CAPTURE_ZERO(script_move.path.count);NPC_CAPTURE_ZERO(navigation.retained.count);
    if(!o->script_animation.active&&o->script_animation.freeze)
        return scene_npc_checkpoint_reject(index,"inactive_script_freeze",0,o->script_animation.freeze,RF_RANGE);
    if(o->view.linked_handle!=-1)return scene_npc_checkpoint_reject(index,"linked_handle",0,o->view.linked_handle,RF_RANGE);
    NPC_CAPTURE_ZERO(view.occupant_count);
    if(o->attachment_75c!=-1)return scene_npc_checkpoint_reject(index,"attachment_75c",0,o->attachment_75c,RF_RANGE);
    NPC_CAPTURE_ZERO(controller_handle);NPC_CAPTURE_ZERO(combat_alert);NPC_CAPTURE_ZERO(combat_scripted);
    NPC_CAPTURE_ZERO(combat_target);NPC_CAPTURE_ZERO(combat_burst_remaining);NPC_CAPTURE_ZERO(combat_due);
    NPC_CAPTURE_ZERO(combat_reload_due);NPC_CAPTURE_ZERO(combat_navigation_due);NPC_CAPTURE_ZERO(damage.effects.burn);
#undef NPC_CAPTURE_ZERO
    for(i=0;i<64;i++)if(o->firing.active[i])return scene_npc_checkpoint_reject(index,"firing.active",i,o->firing.active[i],RF_RANGE);
    if(o->registration.view){
        for(i=0;i<5;i++){status=rf_timer_pending(deadlines[i],now,&pending);
            if(status||pending)return scene_npc_checkpoint_reject(index,"deadline",i,deadlines[i],status?status:RF_RANGE);}
        if(index>=campaign_poses.count||!campaign_poses.items)return RF_FORMAT;pose=campaign_poses.items+index;
        if(pose->playback.completion.active.count>16)return scene_npc_checkpoint_reject(index,"motion_count",0,pose->playback.completion.active.count,RF_RANGE);
        if(!initializing&&!pose->playback.completion.active.count&&!o->script_animation.active&&
           (pose->controller.override_enabled||pose->controller.duration!=0||pose->controller.current>0||pose->controller.next>0))
            return scene_npc_checkpoint_reject(index,"controller_without_playback",0,pose->controller.current,RF_RANGE);
        if(pose->playback.completion.frozen&&!o->script_animation.active)return scene_npc_checkpoint_reject(index,"motion_frozen",0,pose->playback.completion.frozen,RF_RANGE);

        /* Preserve ambient controller loops as well as scripted playback.
         * Non-script actions remain excluded until their gameplay owner is saved. */
        for(i=0;i<pose->playback.completion.active.count;i++){
            int32_t motion=pose->playback.completion.active.slots[i].motion;
            uint32_t mapped=0,k;
            for(k=0;k<3;k++){
                int32_t state=k==0?pose->controller.current:k==1?pose->controller.next:
                    pose->controller.override_enabled?pose->controller.override_state:-1;
                if(state>=0&&state<23&&o->selection.mapping.states[state]==motion)mapped=1;
            }
            if(key->retired||(!initializing&&!o->script_animation.active&&!mapped)||motion<0||
               !campaign_playback_resources.models||pose->skeleton>=campaign_playback_resources.model_count||
               (uint32_t)motion>=campaign_playback_resources.models[pose->skeleton].count||
               !campaign_playback_resources.models[pose->skeleton].resources||
               (!initializing&&!o->script_animation.active&&!campaign_playback_resources.models[pose->skeleton].resources[motion].looping))
                return scene_npc_checkpoint_reject(index,"motion_not_idle_loop",(uint32_t)o->selection.mapping.states[0],motion,RF_RANGE);
        }
        /* rf_entity_body_open deliberately supports an empty authored sphere
         * set (Cutter/fish): it still owns a body and installs an identity tensor.
         * Capture its real pose, never invent a collision sphere or body. */
        if(!o->body.allocated_bytes)return scene_npc_checkpoint_reject(index,"body_missing",0,0,RF_RANGE);
        if(o->body.spheres.count>8||(o->body.spheres.count&&!o->body.spheres.items)||
           (!o->body.spheres.count&&o->body.spheres.items))
            return scene_npc_checkpoint_reject(index,"body_sphere_storage",0,o->body.spheres.count,RF_FORMAT);
        for(i=0;i<3;i++){
            if(!isfinite(o->body.state.velocity[i])||fabsf(o->body.state.velocity[i])>.001f)
                return scene_npc_checkpoint_reject(index,"velocity",i,o->body.state.velocity[i],RF_RANGE);
#define NPC_CAPTURE_AXIS(field) do{if(o->field[i]!=0)return scene_npc_checkpoint_reject(index,#field,i,o->field[i],RF_RANGE);}while(0)
            NPC_CAPTURE_AXIS(command_714);NPC_CAPTURE_AXIS(look.command_708);NPC_CAPTURE_AXIS(support_velocity);
            NPC_CAPTURE_AXIS(look.angles.delta_888);NPC_CAPTURE_AXIS(look.angles.offset_894);
            NPC_CAPTURE_AXIS(look.angles.vector_870);
#undef NPC_CAPTURE_AXIS
        }
        if(o->script_animation.active){
            int32_t motion=o->script_animation.motion;const rf_entity_playback_model *model;
            if(o->script_animation.active!=1||o->script_animation.loop>1||o->script_animation.freeze>1||motion<0||
               !campaign_playback_resources.models||pose->skeleton>=campaign_playback_resources.model_count)return RF_FORMAT;
            model=campaign_playback_resources.models+pose->skeleton;
            if(!model->resources||(uint32_t)motion>=model->count||model->resources[motion].looping!=o->script_animation.loop)return RF_FORMAT;
        }
    }
    for(i=0;i<3;i+=2)if(o->look.body_angles[i]!=0)return scene_npc_checkpoint_reject(index,"body_pitch_roll",i,o->look.body_angles[i],RF_RANGE);
    memset(row,0,sizeof(*row));row->uid=key->uid;row->class_id=campaign_seeds.items[index].class_index;
    row->retired=key->retired;row->flags=o->object_flags&0x4004u;row->affiliation=o->damage.effects.affiliation;
    row->health=o->damage.effects.health;row->armor=o->damage.effects.armor;
    memcpy(row->position,o->registration.view?o->body.state.position:o->published,12);
    row->yaw=o->look.body_angles[1];memcpy(row->eye_angles,o->look.angles.angles_87c,12);
    row->primary=o->view.weapons[0];row->secondary=o->view.weapons[1];
    row->drop=rf_scene_defeated_actors.drops[slot];row->inventory=o->inventory;row->ai_mode=o->ai_mode.action_280;
    if(o->registration.view&&(o->script_animation.active||campaign_poses.items[index].playback.completion.active.count)){
        if(key->retired)return RF_RANGE;
        row->animation_present=1;row->script_animation.active=o->script_animation.active;
        row->script_animation.loop=o->script_animation.active?o->script_animation.loop:0;row->script_animation.freeze=o->script_animation.active?o->script_animation.freeze:0;
        row->script_animation.motion=o->script_animation.active?o->script_animation.motion:0;
        row->playback=campaign_poses.items[index].playback;row->controller=campaign_poses.items[index].controller;
        memset(row->playback.completion.active.slots+row->playback.completion.active.count,0,
            (16-row->playback.completion.active.count)*sizeof(row->playback.completion.active.slots[0]));
    }
    return RF_OK;
}
static int scene_npc_checkpoint_row(uint32_t index,int32_t now,rf_npc_checkpoint_record *row)
{return scene_npc_checkpoint_row_mode(index,now,row,0);}
static int scene_npc_checkpoint_row_compare(const void *a,const void *b)
{uint32_t x=((const rf_npc_checkpoint_record*)a)->uid,y=((const rf_npc_checkpoint_record*)b)->uid;return x<y?-1:x>y;}
static int scene_npc_checkpoint_capture(const rf_npc_checkpoint_catalog *catalog,int32_t now,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{
    rf_npc_checkpoint_record row;unsigned char encoded[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW_MAX],identity[32]={0};
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
