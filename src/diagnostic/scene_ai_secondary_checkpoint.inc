#ifndef RF_SCENE_AI_SECONDARY_CHECKPOINT_INCLUDED
#define RF_SCENE_AI_SECONDARY_CHECKPOINT_INCLUDED
/* Original406330/406390/426ca0 owns the independent secondary deadline;
 * Shoot_Once4badd5..4baddd forces a new accepted Tankbot shot through it.
 * RFAP9 preserves the implemented paid wait, never replays either launch.
 * Include after the full RFAP stage, source helper and delayed candidate join. */
static int scene_ai_secondary_checkpoint_profile(uint32_t kind,int32_t *weapon,
    const rf_weapon_explosive_definition **flight,const rf_weapon_primary_definition **primary)
{
    const rf_weapon_acquire_definition *ammo;uint32_t drone=kind==21;
    if(kind!=21&&kind!=22)return RF_FORMAT;
    if(!(drone?scene_drone_missile_resources:scene_tankbot_missile_resources)||
       !campaign_weapon_supply.names.count||campaign_weapon_supply.names.count>64)return RF_FORMAT;
    *weapon=rf_weapon_name_find(&campaign_weapon_supply.names,drone?"Drone Missile":"Tankbot Missile");
    if(*weapon<0||*weapon>=64||(uint32_t)*weapon>=campaign_weapon_supply.names.count||
       (uint32_t)*weapon<campaign_weapon_supply.names.primary_count)return RF_FORMAT;
    *flight=drone?&campaign_drone_missile:&campaign_tankbot_missile;
    *primary=drone?&campaign_drone_missile_primary:&campaign_tankbot_missile_primary;
    ammo=campaign_weapon_supply.definitions+*weapon;
    if(ammo->ammo_type<0||ammo->ammo_type>=32||ammo->magazine||ammo->capacity!=(drone?12:36)||
       (*primary)->magazine||(*primary)->reload_seconds!=0||(*primary)->projectiles!=1||
       (*primary)->fire_seconds!=4.f||(*primary)->damage_kind!=3||(*primary)->damage!=(drone?15.f:25.f)||
       (*primary)->ai_attack_range!=(drone?30.f:150.f)||(*flight)->speed!=(drone?20.f:15.f)||
       (*flight)->lifetime!=(drone?5.f:6.f)||(*flight)->damage_radius!=(drone?2.f:3.f)||
       (*flight)->crater_radius!=5.f||!isfinite((*flight)->collision_radius)||
       (*flight)->collision_radius<=0||(*flight)->collision_radius>10)return RF_FORMAT;
    return RF_OK;
}
static int scene_ai_secondary_checkpoint_source(uint32_t source,
    scene_ai_secondary_checkpoint_owner *owner)
{
    uint32_t found=0;
    if(!owner||!source||source==UINT32_MAX||!campaign_npc_bodies||!campaign_seeds.records.items||
       campaign_seeds.records.count<campaign_npc_body_count||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==source){
        const campaign_npc_body *npc=scene_ai_tankbot_secondary_identity(i,source);
        const rf_campaign_object_record *key;uint32_t uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
        /* Forced Tankbot shots require registry/entity/eye/ammo ownership, not
         * an allocated physics body. Do not narrow that existing launch path. */
        if(found++||!npc||!npc->persistence_registered||npc->persistence_slot>=rf_scene_defeated_actors.count||
           npc->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
        key=rf_scene_defeated_actors.items+npc->persistence_slot;
        if(!uid||uid>(uint32_t)INT32_MAX||key->uid!=uid||key->retired||
           key->level>=rf_scene_defeated_actors.level_count||key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
           !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level))return RF_FORMAT;
        owner->uid=uid;owner->source=source;owner->source_slot=i;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_ai_secondary_checkpoint_source_uid(uint32_t source,uint32_t *uid)
{
    scene_ai_secondary_checkpoint_owner owner={0};int status;
    if(!uid)return RF_RANGE;
    status=scene_ai_secondary_checkpoint_source(source,&owner);if(!status)*uid=owner.uid;return status;
}
static int scene_ai_secondary_checkpoint_capture(unsigned char *rows,uint32_t *count)
{
    uint32_t n=0,tankbot_count=0;int32_t now;int status;
    if(!count)return RF_RANGE;
    if(scene_ai_secondary_checkpoint_busy())return RF_NOT_FOUND;
    if(combat_frame==UINT32_MAX)return RF_RANGE;
    if(campaign_npc_body_count&&!campaign_npc_bodies)return RF_FORMAT;
    now=scene_ai_drone_secondary_now(combat_frame);
    for(uint32_t kind=21;kind<=22;kind++){
        uint32_t limit=kind==21?SCENE_AI_DRONE_SECONDARY_CAPACITY:campaign_npc_body_count;
        for(uint32_t i=0;i<limit;i++){
            scene_ai_secondary_checkpoint_owner o={0};int32_t deadline,weapon;
            const rf_weapon_explosive_definition *flight;const rf_weapon_primary_definition *primary;
            const campaign_npc_body *npc;unsigned char *r;uint32_t slot=i;
            if(kind==21){
                const scene_ai_drone_secondary_cooldown *live=scene_ai_drone_secondary_cooldowns+i;
                if(!live->phase)continue;
                if(live->phase!=2||!live->ticket)return RF_FORMAT;
                o.source=live->source;deadline=live->deadline;
            }else{
                npc=campaign_npc_bodies+i;
                /* Match service: a superseded handle no longer owns a wait. */
                if(!npc->tankbot_secondary_source||npc->tankbot_secondary_source!=npc->registration.handle)continue;
                o.source=npc->tankbot_secondary_source;deadline=npc->tankbot_secondary_deadline;
            }
            status=rf_timer_remaining(deadline,now,&o.remaining_ms);if(status)return status;
            if(o.remaining_ms<=0)continue; /* Read-only expiry, no live reclamation. */
            if(o.remaining_ms>4000)return RF_FORMAT;
            status=scene_ai_secondary_checkpoint_profile(kind,&weapon,&flight,&primary);if(status)return status;
            status=scene_ai_secondary_checkpoint_source(o.source,&o);if(status)return status;
            npc=campaign_npc_bodies+o.source_slot;
            o.reserve=npc->inventory.reserve[campaign_weapon_supply.definitions[weapon].ammo_type];
            if(o.reserve<0)return RF_FORMAT;
            if(kind==22){if(tankbot_count>=SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY)return RF_RANGE;slot=tankbot_count++;}
            if(!rows){++n;continue;}r=rows+112*n++;
            checkpoint_put(r,kind);checkpoint_put(r+4,slot);checkpoint_put(r+8,o.uid);
            checkpoint_put(r+16,(uint32_t)o.remaining_ms);checkpoint_put(r+20,(uint32_t)o.reserve);
        }
    }
    *count=n;return RF_OK;
}
static int scene_ai_secondary_checkpoint_read(scene_ai_secondary_checkpoint_stage *stage,const unsigned char *r)
{
    uint32_t kind=checkpoint_u32(r),slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),source;
    scene_ai_secondary_checkpoint_owner next={0},*bank;int32_t weapon;
    const rf_weapon_explosive_definition *flight;const rf_weapon_primary_definition *primary;
    if(!stage||slot>=SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY||!uid||uid>(uint32_t)INT32_MAX||
       checkpoint_u32(r+12)||scene_ai_secondary_checkpoint_profile(kind,&weapon,&flight,&primary))return RF_FORMAT;
    for(uint32_t i=24;i<112;i++)if(r[i])return RF_FORMAT;
    next.remaining_ms=(int32_t)checkpoint_u32(r+16);next.reserve=(int32_t)checkpoint_u32(r+20);
    if(next.remaining_ms<=0||next.remaining_ms>4000||next.reserve<0||
       scene_ai_projectile_source_handle(uid,&source)||scene_ai_secondary_checkpoint_source(source,&next)||next.uid!=uid)return RF_FORMAT;
    bank=kind==21?stage->drone:stage->tankbot;
    if(bank[slot].uid)return RF_FORMAT;
    for(uint32_t i=0;i<SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY;i++)if(bank[i].uid==uid)return RF_FORMAT;
    bank[slot]=next;if(kind==21)++stage->drone_count;else ++stage->tankbot_count;
    return RF_OK;
}
/* Straight accepted missiles carry raw damage and their family, independently
 * of the source's current selection/life and independently of cooldown expiry. */
static int scene_ai_secondary_checkpoint_missile_shape(const scene_stream *stream,uint32_t variant,
    const scene_ai_rocket_projectile *shot)
{
    const rf_weapon_explosive_definition *flight;const rf_weapon_primary_definition *primary;
    int32_t weapon;uint32_t i,j,k;int status;
    if(variant!=2&&variant!=3)return RF_FORMAT;
    status=scene_ai_secondary_checkpoint_profile(variant==3?21:22,&weapon,&flight,&primary);if(status)return status;
    if(!stream||!stream->rocket_visual||!shot||shot->flight.active!=1||shot->weapon!=weapon||
       shot->definition!=flight||shot->damage!=combat_enemy_primary_damage(primary)||
       shot->flight.radius!=flight->collision_radius||!isfinite(shot->flight.remaining)||
       shot->flight.remaining<=0||shot->flight.remaining>flight->lifetime||
       (shot->liquid.query_flags!=(4u|(flight->collision_radius<.05f?0x100u:0))&&
        shot->liquid.query_flags!=(0x1004u|(flight->collision_radius<.05f?0x100u:0))))return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(shot->flight.position[i])||fabsf(shot->flight.position[i])>100000||
           !isfinite(shot->flight.velocity[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(shot->basis[i*3+j]))return RF_FORMAT;
    }
    for(i=0;i<3;i++){
        if(fabsf(shot->flight.velocity[i]/flight->speed-shot->basis[6+i])>.001f)return RF_FORMAT;
        for(j=i;j<3;j++){
            double dot=0;for(k=0;k<3;k++)dot+=(double)shot->basis[i*3+k]*shot->basis[j*3+k];
            if(fabs(dot-(i==j?1.0:0.0))>.001)return RF_FORMAT;
        }
        j=(i+1)%3;k=(i+2)%3;
        if(fabsf(shot->basis[j]*shot->basis[3+k]-shot->basis[k]*shot->basis[3+j]-shot->basis[6+i])>.001f)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_ai_projectile_checkpoint_secondary_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const scene_npc_checkpoint_restore_stage *npcs)
{
    if(!stage)return RF_OK;
    for(uint32_t i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)if(stage->rocket_source_uid[i]){
        if(!scene_ai_delayed_checkpoint_candidate(npcs,stage->rocket_source_uid[i],stage->rockets[i].source))return RF_FORMAT;
    }
    for(uint32_t kind=21;kind<=22;kind++){
        const scene_ai_secondary_checkpoint_owner *bank=kind==21?stage->secondary.drone:stage->secondary.tankbot;
        for(uint32_t i=0;i<SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY;i++)if(bank[i].uid){
            const scene_ai_secondary_checkpoint_owner *o=bank+i;int32_t weapon;
            const rf_weapon_explosive_definition *flight;const rf_weapon_primary_definition *primary;
            const scene_npc_checkpoint_restore_entry *e=scene_ai_delayed_checkpoint_candidate(npcs,o->uid,o->source);
            if(!e||e->slot!=o->source_slot||scene_ai_secondary_checkpoint_profile(kind,&weapon,&flight,&primary)||
               e->saved.inventory.reserve[campaign_weapon_supply.definitions[weapon].ammo_type]!=o->reserve)return RF_FORMAT;
            /* No currently-held, living, alerted or positive-ammo requirement:
             * accepted cooldowns deliberately outlive those eligibility gates. */
        }
    }
    return RF_OK;
}
static int scene_ai_secondary_checkpoint_complete(const scene_ai_secondary_checkpoint_stage *stage)
{
    if(!stage)return RF_RANGE;
    for(uint32_t i=0;i<SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY;i++)
        if((i<stage->tankbot_count)!=(stage->tankbot[i].uid!=0))return RF_FORMAT;
    return UINT64_MAX-scene_ai_drone_secondary_ticket<stage->drone_count?RF_RANGE:RF_OK;
}
/* Final storage success only, after outgoing secondary resets. Same-slot
 * publication preserves allocation order. No ammo, launch or primary cadence
 * callback runs here; monotonically fresh tickets reject suspended old work. */
static void scene_ai_projectile_checkpoint_secondary_finish(const scene_ai_projectile_checkpoint_stage *stage)
{
    if(!stage)return;
    memcpy(scene_ai_rockets,stage->rockets,sizeof(scene_ai_rockets));rf_scene_ai_rockets[3]=stage->rockets_live;
    for(uint32_t i=0;i<SCENE_AI_SECONDARY_CHECKPOINT_CAPACITY;i++){
        const scene_ai_secondary_checkpoint_owner *o=stage->secondary.drone+i;
        if(o->uid){scene_ai_drone_secondary_cooldown *live=scene_ai_drone_secondary_cooldowns+i;
            live->ticket=++scene_ai_drone_secondary_ticket;live->phase=2;live->source=o->source;
            live->deadline=scene_ai_delayed_checkpoint_rebase(o->remaining_ms);}
        o=stage->secondary.tankbot+i;
        if(o->uid){campaign_npc_body *npc=campaign_npc_bodies+o->source_slot;
            npc->tankbot_secondary_source=o->source;npc->tankbot_secondary_deadline=scene_ai_delayed_checkpoint_rebase(o->remaining_ms);}
    }
}
#endif
