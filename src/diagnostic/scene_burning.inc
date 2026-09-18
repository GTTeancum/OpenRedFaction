#ifndef RF_SCENE_BURNING_INC
#define RF_SCENE_BURNING_INC
/* Persistent NPC burning without emitter/audio ownership. Existing damage
 * admission excludes player ignition and determines armor/fire immunity.
 * Damage rate follows retained42f1dc/rf_burn_owner_tick: class_health/divisor,
 * with deterministic midpoint6.5 instead of original random5..8. Five-second
 * lifetime and quarter-second batching are explicit first-pass port policy.
 * Include after combat feedback callbacks and combat_death_start. */
#define SCENE_BURNING_CAPACITY 64u
typedef struct scene_burning_record {uint32_t target,source,left,phase;} scene_burning_record;
static scene_burning_record scene_burning_records[SCENE_BURNING_CAPACITY];
/* ignitions, damage pulses, retirements, deaths, capacity refusals */
uint32_t rf_scene_burning[5];
static campaign_npc_body *scene_burning_owner(uint32_t target,uint32_t *index)
{
    uint32_t i;for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if(owner->registration.view && owner->registration.handle==target &&
           rf_object_registry_lookup(&campaign_registry,target)==&owner->registration){if(index)*index=i;return owner;}
    }return NULL;
}
static void scene_burning_retire(uint32_t slot)
{
    scene_burning_record *r=scene_burning_records+slot;campaign_npc_body *owner;
    if(!r->left)return;owner=scene_burning_owner(r->target,NULL);
    if(owner && owner->damage.effects.burn==slot+1){owner->damage.effects.burn=0;owner->damage.burn_source=UINT32_MAX;}
    memset(r,0,sizeof(*r));++rf_scene_burning[2];
}
static void scene_burning_extinguish(uint32_t target)
{uint32_t i;for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(scene_burning_records[i].left && scene_burning_records[i].target==target)scene_burning_retire(i);}
static void scene_burning_release_all(void)
{uint32_t i;for(i=0;i<SCENE_BURNING_CAPACITY;i++)scene_burning_retire(i);}
static void scene_burning_reset(void)
{scene_burning_release_all();memset(rf_scene_burning,0,sizeof(rf_scene_burning));}
static uint32_t scene_burning_save_pending(void)
{uint32_t i;for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(scene_burning_records[i].left)return 1;return 0;}
static uint32_t scene_burning_create(uint32_t target,uint32_t source)
{
    campaign_npc_body *owner=scene_burning_owner(target,NULL);uint32_t i;
    if(!owner || owner->damage.effects.health<=0 || !isfinite(owner->damage.effects.class_health) ||
       owner->damage.effects.class_health<=0 || (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u))return 0;
    for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(scene_burning_records[i].left && scene_burning_records[i].target==target)return i+1;
    for(i=0;i<SCENE_BURNING_CAPACITY && scene_burning_records[i].left;i++){}
    if(i==SCENE_BURNING_CAPACITY){++rf_scene_burning[4];return 0;}
    scene_burning_records[i]=(scene_burning_record){target,source,300,0};owner->damage.burn_source=source;
    ++rf_scene_burning[0];return i+1;
}
static int scene_burning_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,bits;float seconds=(float)frame/60;int status;
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    if(!stream)return RF_RANGE;memcpy(&bits,&seconds,4);
    for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(scene_burning_records[i].left){
        scene_burning_record *r=scene_burning_records+i;uint32_t index,entered;float amount=0;
        campaign_npc_body *owner=scene_burning_owner(r->target,&index);
        if(!owner || owner->damage.effects.burn!=i+1 || owner->damage.effects.health<=0 ||
           (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u)){scene_burning_retire(i);continue;}
        if(stream->collision && stream->liquid_rooms){rf_collision_room_location location;
            status=rf_geometry_collision_world_locate(stream->collision,owner->body.state.position,&location);if(status)return status;
            if(location.room<stream->swim_room_count){const rf_liquid_room *liquid=stream->liquid_rooms+location.room;
                /* Ordinary nonhazardous liquid extinguishes at actor origin;
                 * lava2 and acid3 do not. Dry rooms carry nonfinite depth. */
                if(liquid->type>=0 && liquid->type<2 && isfinite(liquid->depth) &&
                   owner->body.state.position[1]<=liquid->minimum_y+liquid->depth){scene_burning_retire(i);continue;}
            }
        }
        if(++r->phase==15){rf_damage_request request={0};r->phase=0;
            request.amount=owner->damage.effects.class_health*(.25f/6.5f);request.kind=4;request.source=r->source;request.auxiliary_uid=UINT32_MAX;
            owner->damage.burn_source=r->source;
            status=rf_scene_npc_damage(r->target,&request,1,bits,&effects,&amount);if(status)return status;if(feedback.status)return feedback.status;
            ++rf_scene_burning[1];
            if(owner->damage.effects.health<=0){uint32_t target=r->target;scene_burning_retire(i);
                status=rf_scene_npc_death_entry(target,&entered);if(status)return status;
                if(entered){++rf_scene_burning[3];status=combat_death_start(index);if(status)return status;}
                continue;
            }
        }
        if(r->left==1)scene_burning_retire(i);else --r->left;
    }
    return RF_OK;
}
#endif
