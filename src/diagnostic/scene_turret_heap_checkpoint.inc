#ifndef RF_SCENE_TURRET_HEAP_CHECKPOINT_INCLUDED
#define RF_SCENE_TURRET_HEAP_CHECKPOINT_INCLUDED
/* RFAP11 kind25: paid HEAP flight, authored turret host, typed damage source
 * and independent launch-time collision exclusion. No seat relation, current
 * weapon selection, runtime handle or new shot is reconstructed from this row. */
static int scene_turret_heap_checkpoint_player(uint32_t *handle)
{
    uint32_t value=campaign_player_object.handle;
    if(!handle||!value||value==UINT32_MAX||!scene_actor_body.allocated_bytes||
       campaign_player_object.view!=&campaign_player_view||
       (uint32_t)campaign_player_view.handle!=value||
       campaign_player_damage.state.effects.handle!=value||
       rf_object_registry_lookup(&campaign_registry,value)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)value)!=&campaign_player_view)return RF_FORMAT;
    *handle=value;return RF_OK;
}
/* Host identity remains valid after turret death. RFTU retains that dead
 * owner; requiring it to be alive would silently change paid-flight lifetime. */
static int scene_turret_heap_checkpoint_host(uint32_t value,uint32_t decode,uint32_t *out)
{
    uint32_t found=0,result=0;
    if(!out||!value||value==UINT32_MAX||decode>1||
       scene_turret_authored_count>scene_turret_count||scene_turret_count>SCENE_TURRET_OWNER_LIMIT||
       (scene_turret_count&&!scene_turrets)||!campaign_seeds.records.items||!campaign_seeds.items||
       !campaign_seeds.classes)return RF_FORMAT;
    for(uint32_t i=0;i<scene_turret_authored_count;i++){
        const scene_turret_owner *o=scene_turrets+i;
        if(value!=(decode?o->uid:o->registration.handle))continue;
        if(found++||!o->uid||o->uid>(uint32_t)INT32_MAX||!o->registration.handle||
           o->registration.handle==UINT32_MAX||scene_turret_lookup(o->registration.handle)!=o||
           (uint32_t)o->view.handle!=o->registration.handle||o->damage.effects.handle!=o->registration.handle||
           o->seed>=campaign_seeds.records.count||o->class_index>=campaign_seeds.class_count||
           (uint32_t)campaign_seeds.records.items[o->seed].record.uid!=o->uid||
           campaign_seeds.items[o->seed].class_index!=o->class_index)return RF_FORMAT;
        result=decode?o->registration.handle:o->uid;
    }
    if(found!=1)return RF_NOT_FOUND; /* Generated heads have no authored host lane. */
    *out=result;return RF_OK;
}
static int scene_turret_heap_checkpoint_shape(const scene_turret_heap_round *shot,uint32_t player_source)
{
    const rf_weapon_primary_definition *primary=&scene_turret_heap_primary;
    const rf_weapon_explosive_definition *explosive=&scene_turret_heap_explosive;
    double speed2=0;float damage,impact;
    if(!shot||player_source>1||!scene_turret_combat_ready||scene_turret_heap_id<0||scene_turret_heap_id>=64||
       rf_weapon_name_find(&campaign_weapon_supply.names,"HEAP")!=scene_turret_heap_id||
       primary->projectiles!=1||primary->damage_kind!=3||primary->magazine||primary->reload_seconds||
       !isfinite(primary->damage)||primary->damage<0||!isfinite(primary->ai_damage_scale[0])||primary->ai_damage_scale[0]<0||
       !isfinite(explosive->speed)||explosive->speed<=0||!isfinite(explosive->lifetime)||explosive->lifetime<=0||
       !isfinite(explosive->collision_radius)||explosive->collision_radius<=0)return RF_FORMAT;
    damage=player_source?primary->damage:combat_enemy_primary_damage(primary);
    impact=explosive->impact_count?explosive->impact_radius[0]:2.f;
    if(shot->weapon!=scene_turret_heap_id||shot->flight.active!=1||
       shot->flight.radius!=explosive->collision_radius||
       !isfinite(shot->flight.remaining)||shot->flight.remaining<=0||shot->flight.remaining>explosive->lifetime||
       !isfinite(shot->damage)||shot->damage<0||shot->damage!=damage||
       !isfinite(shot->blast)||shot->blast<0||shot->blast!=explosive->damage_radius||
       !isfinite(shot->crater)||shot->crater<0||shot->crater!=explosive->crater_radius||
       !isfinite(shot->impact)||shot->impact<0||shot->impact!=impact)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(shot->flight.position[i])||fabsf(shot->flight.position[i])>100000||
           !isfinite(shot->flight.velocity[i]))return RF_FORMAT;
        speed2+=(double)shot->flight.velocity[i]*shot->flight.velocity[i];
    }
    /* HEAP's existing straight-flight owner has neither gravity nor homing.
     * Admit normalization roundoff, retaining all actual accepted float bits. */
    return fabs(sqrt(speed2)/explosive->speed-1)>.001?RF_FORMAT:RF_OK;
}
static int scene_turret_heap_checkpoint_capture(unsigned char *rows,uint32_t *count)
{
    uint32_t n=0;int status;
    if(!count)return RF_RANGE;
    if(scene_turret_heap_checkpoint_busy())return RF_NOT_FOUND;
    for(uint32_t i=0;i<SCENE_TURRET_HEAP_CAPACITY;i++){
        const scene_turret_heap_round *shot=scene_turret_heap_rounds+i;
        uint32_t uid,source_role,excluded=0,operator_uid=0,player=0;
        if(!shot->flight.active)continue;
        status=scene_turret_heap_checkpoint_host(shot->host,0,&uid);if(status)return status;
        if(shot->source==shot->host)source_role=1;
        else{
            status=scene_turret_heap_checkpoint_player(&player);if(status)return status;
            if(shot->source!=player)return RF_FORMAT;source_role=2;
        }
        if(shot->operator_handle!=UINT32_MAX){
            if(shot->operator_handle==campaign_player_object.handle){
                status=scene_turret_heap_checkpoint_player(&player);if(status)return status;excluded=1;
            }else{
                status=scene_ai_fusion_checkpoint_source(shot->operator_handle,&operator_uid);if(status)return status;excluded=2;
            }
        }
        if((source_role==2&&excluded!=1)||(source_role==1&&excluded==1))return RF_FORMAT;
        status=scene_turret_heap_checkpoint_shape(shot,source_role==2);if(status)return status;
        if(rows){unsigned char *r=rows+112*n;
            checkpoint_put(r,25);checkpoint_put(r+4,i);checkpoint_put(r+8,uid);checkpoint_put(r+12,source_role);
            for(uint32_t k=0;k<3;k++){
                checkpoint_put_float(r+16+k*4,shot->flight.position[k]);checkpoint_put_float(r+28+k*4,shot->flight.velocity[k]);
            }
            checkpoint_put_float(r+40,shot->flight.radius);checkpoint_put(r+44,excluded);
            scene_ai_projectile_put_double(r+48,shot->flight.remaining);
            checkpoint_put_float(r+56,shot->damage);checkpoint_put_float(r+60,shot->blast);
            checkpoint_put_float(r+64,shot->crater);checkpoint_put_float(r+68,shot->impact);checkpoint_put(r+72,operator_uid);
        }
        ++n;
    }
    *count=n;return RF_OK;
}
static int scene_turret_heap_checkpoint_read(scene_ai_projectile_checkpoint_stage *stage,const unsigned char *r)
{
    uint32_t slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),role=checkpoint_u32(r+12);
    uint32_t excluded=checkpoint_u32(r+44),operator_uid=checkpoint_u32(r+72),resolved_uid,player=0;
    scene_turret_heap_round *shot;int status;
    if(!stage||checkpoint_u32(r)!=25||slot>=SCENE_TURRET_HEAP_CAPACITY||
       stage->heap[slot].flight.active||role<1||role>2||excluded>2||
       (role==2&&excluded!=1)||(role==1&&excluded==1)||
       (excluded!=2&&operator_uid)||(excluded==2&&(!operator_uid||operator_uid>(uint32_t)INT32_MAX)))return RF_FORMAT;
    for(uint32_t k=76;k<112;k++)if(r[k])return RF_FORMAT;
    shot=stage->heap+slot;
    status=scene_turret_heap_checkpoint_host(uid,1,&shot->host);if(status)return status;
    shot->source=shot->host;shot->operator_handle=UINT32_MAX;
    if(role==2){
        status=scene_turret_heap_checkpoint_player(&player);if(status)return status;
        shot->source=shot->operator_handle=player;
    }else if(excluded==2){
        status=scene_ai_projectile_source_handle(operator_uid,&shot->operator_handle);if(status)return status;
        status=scene_ai_fusion_checkpoint_source(shot->operator_handle,&resolved_uid);if(status)return status;
        if(resolved_uid!=operator_uid||shot->operator_handle==shot->host)return RF_FORMAT;
    }
    shot->weapon=scene_turret_heap_id;shot->flight.active=1;
    shot->flight.radius=checkpoint_float(r+40);shot->flight.remaining=scene_ai_projectile_double(r+48);
    shot->damage=checkpoint_float(r+56);shot->blast=checkpoint_float(r+60);
    shot->crater=checkpoint_float(r+64);shot->impact=checkpoint_float(r+68);
    for(uint32_t k=0;k<3;k++){
        shot->flight.position[k]=checkpoint_float(r+16+k*4);shot->flight.velocity[k]=checkpoint_float(r+28+k*4);
    }
    status=scene_turret_heap_checkpoint_shape(shot,role==2);if(status)return status;
    stage->heap_host_uid[slot]=uid;stage->heap_operator_uid[slot]=operator_uid;++stage->heap_live;return RF_OK;
}
static int scene_turret_heap_checkpoint_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_turret_checkpoint_stage *turrets,
    const scene_world_player_stage *player)
{
    if(!stage||!stage->heap_live)return RF_OK;
    if(!turrets||turrets->count!=scene_turret_authored_count)return RF_FORMAT;
    for(uint32_t i=0;i<SCENE_TURRET_HEAP_CAPACITY;i++)if(stage->heap[i].flight.active){
        const scene_turret_heap_round *shot=stage->heap+i;uint32_t handle,found=0;
        int status=scene_turret_heap_checkpoint_host(stage->heap_host_uid[i],1,&handle);if(status)return status;
        if(handle!=shot->host)return RF_FORMAT;
        for(uint32_t j=0;j<turrets->count;j++)if(turrets->rows[j].uid==stage->heap_host_uid[i]){
            const scene_turret_saved *row=turrets->rows+j;
            if(found++||row->handle!=handle||scene_turrets[j].uid!=row->uid||
               scene_turrets[j].registration.handle!=handle||scene_turrets[j].class_index!=row->class_index)return RF_FORMAT;
            /* Saved turret death/current weapon/occupancy do not cancel a paid
             * flight. RFTU separately validates those saved owner properties. */
        }
        if(found!=1)return RF_FORMAT;
        if(shot->source!=shot->host){
            if(scene_turret_heap_checkpoint_player(&handle)||shot->source!=handle||shot->operator_handle!=handle||
               !player||!isfinite(player->player.player.health)||player->player.player.health<=0)return RF_FORMAT;
        }
        if(stage->heap_operator_uid[i]){
            const scene_npc_checkpoint_restore_entry *operator_owner=NULL;
            if(!npcs)return RF_FORMAT;
            for(uint32_t j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==stage->heap_operator_uid[i]){
                if(operator_owner)return RF_FORMAT;operator_owner=npcs->entries+j;
            }
            /* Keep launch-time exclusion even after dismount or registered
             * corpse transition. Retired/missing owners need a different lane. */
            if(!operator_owner||!operator_owner->registered||operator_owner->saved.retired||
               !operator_owner->handle||operator_owner->handle==UINT32_MAX||operator_owner->handle!=shot->operator_handle||
               operator_owner->slot>=campaign_npc_body_count||
               (uint32_t)campaign_seeds.records.items[operator_owner->slot].record.uid!=stage->heap_operator_uid[i])return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* The sole publisher runs after storage close and successful world ownership
 * assignment. Legacy/absent rows clear this pool; failed staging never does. */
static void scene_turret_heap_checkpoint_finish(const scene_ai_projectile_checkpoint_stage *stage)
{
    scene_turret_heap_reset();if(!stage)return;
    memcpy(scene_turret_heap_rounds,stage->heap,sizeof(scene_turret_heap_rounds));
    rf_scene_turret_heap[3]=stage->heap_live;
}
#endif
