#ifndef RF_SCENE_TURRET_GENERATED_CHECKPOINT_ADAPTER_INCLUDED
#define RF_SCENE_TURRET_GENERATED_CHECKPOINT_ADAPTER_INCLUDED
/* Include after generated checkpoint primitives and ordinary world/player,
 * NPC, passive and authored turret stage types. No published owner mutation. */
typedef struct scene_turret_generated_world_context {
    const scene_npc_checkpoint_restore_stage *npcs;
    const scene_world_player_stage *player;
    const scene_vehicle_checkpoint_record *vehicle;
    const scene_passive_vehicle_checkpoint_stage *passive;
    const scene_turret_checkpoint_stage *turrets;
    const scene_turret_generated_checkpoint_stage *generated;
    uint32_t vehicle_uid;
} scene_turret_generated_world_context;

static int scene_turret_generated_world_base(void *context,uint32_t uid,
    scene_turret_generated_base_saved *out)
{
    const scene_turret_generated_world_context *c=context;uint32_t i;
    if(!c||!c->npcs||!out||uid==UINT32_MAX)return RF_RANGE;
    for(i=0;i<c->npcs->count;i++) {
        const scene_npc_checkpoint_restore_entry *e=c->npcs->entries+i;
        const rf_npc_checkpoint_record *r=&e->saved;
        if(r->uid!=uid)continue;
        if(!scene_turret_generated_is_base(e->slot))return RF_NOT_FOUND;
        if(!isfinite(r->health))return RF_FORMAT;
        out->health=r->health;out->flags_810=r->health<=0?1u:0;
        out->flags_7d0=r->ai_suppressed?SCENE_NPC_GOTO_AI_SUPPRESSION:0;
        out->retired=r->retired;return RF_OK;
    }
    return RF_NOT_FOUND;
}

static int scene_turret_generated_world_target(void *context,uint32_t role,uint32_t uid,uint32_t *alive)
{
    const scene_turret_generated_world_context *c=context;uint32_t i;
    if(!c||!c->npcs||!alive||uid==UINT32_MAX||role>SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD)return RF_RANGE;
    if(role==SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD) {
        if(!c->generated)return RF_NOT_FOUND;
        for(i=0;i<c->generated->count;i++) {
            const scene_turret_generated_saved *r=c->generated->rows+i;
            if(r->key.base_uid!=uid||r->key.role!=role)continue;
            *alive=!r->value.dead&&isfinite(r->value.health)&&r->value.health>0&&
                !(r->value.flags_810&1u)&&!(r->value.flags&(2u|0x4000u));return RF_OK;
        }
        return RF_NOT_FOUND;
    }
    /* Match the ordinary identity codec's category precedence. Finding a dead
     * owner is final; never fall through and authorize a different category. */
    if(!uid) {
        if(!c->player)return RF_NOT_FOUND;
        *alive=isfinite(c->player->player.player.health)&&c->player->player.player.health>0;return RF_OK;
    }
    if(c->vehicle_uid&&uid==c->vehicle_uid) {
        if(!c->vehicle)return RF_NOT_FOUND;
        *alive=c->vehicle->value.vehicle.alive==1&&isfinite(c->vehicle->value.vehicle.health)&&
            c->vehicle->value.vehicle.health>0;return RF_OK;
    }
    for(i=0;i<c->npcs->count;i++) {
        const rf_npc_checkpoint_record *r=&c->npcs->entries[i].saved;
        if(r->uid!=uid)continue;
        *alive=!r->retired&&!r->dead_pose&&isfinite(r->health)&&r->health>0&&
            !(c->npcs->entries[i].flags&(2u|0x4000u));return RF_OK;
    }
    if(c->passive)for(i=0;i<c->passive->count;i++) {
        const scene_passive_vehicle_saved *r=c->passive->rows+i;
        if(r->uid!=uid)continue;
        /* RFVA1 has no saved vitals. Do not substitute fresh live health. */
        if(!r->has_damage)return RF_NOT_FOUND;
        *alive=!r->destroyed&&isfinite(r->health)&&r->health>0&&
            !(r->flags_810&1u)&&!(r->object_flags&(2u|0x4000u));return RF_OK;
    }
    if(c->turrets)for(i=0;i<c->turrets->count;i++) {
        const scene_turret_saved *r=c->turrets->rows+i;
        if(r->uid!=uid)continue;
        *alive=!r->dead&&isfinite(r->health)&&r->health>0&&
            !(r->flags_810&1u)&&!(r->flags&(2u|0x4000u));return RF_OK;
    }
    return RF_NOT_FOUND;
}

static int scene_turret_generated_world_admit(const scene_turret_generated_checkpoint_stage *generated,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_world_player_stage *player,
    const scene_vehicle_checkpoint_record *vehicle,uint32_t vehicle_uid,
    const scene_passive_vehicle_checkpoint_stage *passive,const scene_turret_checkpoint_stage *turrets)
{
    scene_turret_generated_world_context c={npcs,player,vehicle,passive,turrets,generated,vehicle_uid};
    return scene_turret_retirement_checkpoint_admit(generated,scene_turret_generated_world_base,
        scene_turret_generated_world_target,&c);
}

/* The NPC slice was already captured at this same paused simulation boundary.
 * Decode within an explicit scratch budget to prove generated base state will
 * agree with RFNC on reload. Free this scratch before generated row capture. */
static int scene_turret_generated_world_capture(unsigned char *p,uint32_t capacity,uint32_t inner_bytes,
    uint32_t frame,uint32_t budget,const unsigned char identity[32],const void *npc_data,uint32_t npc_bytes,
    uint32_t *written)
{
    rf_npc_checkpoint_catalog *catalog=NULL;rf_npc_checkpoint_record *rows=NULL;
    uint32_t count=0,i,j;uint64_t allocated;int status;
    if(!scene_turret_generated_count)
        return scene_turret_retirement_checkpoint_capture(p,capacity,inner_bytes,frame,budget,written);
    if(!identity||!npc_data||!npc_bytes||budget<sizeof(*catalog))return RF_RANGE;
    catalog=calloc(1,sizeof(*catalog));if(!catalog)return RF_IO;
    catalog->hash=rf_scene_weapon_supply[3];catalog->count=campaign_weapon_supply.names.count;
    if(!catalog->count||catalog->count>64){status=RF_RANGE;goto done;}
    memcpy(catalog->weapons,campaign_weapon_supply.definitions,sizeof(catalog->weapons));
    for(i=0;i<catalog->count;i++) {
        const rf_weapon_acquire_definition *d=catalog->weapons+i;
        catalog->supported[i]=(uint8_t)(d->ammo_type>=-1&&d->ammo_type<32&&d->capacity>=0&&d->magazine>=0&&
            (d->ammo_type!=-1||d->magazine==0));
    }
    status=rf_npc_checkpoint_preflight(npc_data,npc_bytes,identity,catalog,&count);if(status)goto done;
    allocated=sizeof(*catalog)+(uint64_t)count*sizeof(*rows);
    if(allocated>budget){status=RF_RANGE;goto done;}
    if(count){rows=malloc(count*sizeof(*rows));if(!rows){status=RF_IO;goto done;}}
    status=rf_npc_checkpoint_decode(npc_data,npc_bytes,identity,catalog,rows,count,&count);if(status)goto done;
    for(i=0;i<scene_turret_generated_count;i++) {
        const scene_turret_generated_binding *b=scene_turret_generated+i;
        scene_turret_generated_base_saved live={0};
        for(j=0;j<count;j++)if(rows[j].uid==b->key.base_uid)break;
        if(j==count){status=RF_NOT_FOUND;goto done;}
        status=scene_turret_retirement_checkpoint_capture_base(NULL,b->key.base_uid,&live);if(status)goto done;
        if(live.health!=rows[j].health||live.retired!=rows[j].retired||
           !!(live.flags_810&1u)!=(rows[j].health<=0)||
           !!(live.flags_7d0&SCENE_NPC_GOTO_AI_SUPPRESSION)!=rows[j].ai_suppressed){
            status=RF_FORMAT;goto done;
        }
    }
    free(rows);rows=NULL;free(catalog);catalog=NULL;
    return scene_turret_retirement_checkpoint_capture(p,capacity,inner_bytes,frame,budget,written);
done:
    free(rows);free(catalog);return status;
}
#endif
