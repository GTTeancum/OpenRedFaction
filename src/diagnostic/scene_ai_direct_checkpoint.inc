#ifndef RF_SCENE_AI_DIRECT_CHECKPOINT_INCLUDED
#define RF_SCENE_AI_DIRECT_CHECKPOINT_INCLUDED
/* RFAP7 kinds13/14 own immediate, already-paid Laser/Sonar flights. The two
 * high resource bits exist only in the NPC mask, never in player view demand.
 * RFNC remains the source inventory/action/cadence owner; no receipt here can
 * discharge unrelated NPC movement, delayed attacks or animation obligations. */
#define SCENE_AI_DIRECT_RESOURCE_LASER (1u<<30)
#define SCENE_AI_DIRECT_RESOURCE_SONAR (1u<<31)
static const scene_ai_laser_profile *scene_ai_direct_checkpoint_profile(uint32_t kind)
{
    const scene_ai_laser_profile *profile=kind==13?&scene_ai_laser_definition:
        kind==14?&scene_ai_sonar_definition:NULL;
    const rf_weapon_acquire_definition *ammo;
    if(!profile||profile->ready!=1||profile->weapon<0||profile->weapon>=64||
       campaign_weapon_supply.names.count>64||(uint32_t)profile->weapon>=campaign_weapon_supply.names.count||
       rf_weapon_name_find(&campaign_weapon_supply.names,kind==13?"Laser":"Sea Creature Sonar Attack")!=profile->weapon)return NULL;
    ammo=campaign_weapon_supply.definitions+profile->weapon;
    if(ammo->ammo_type<0||ammo->ammo_type>=32||ammo->capacity!=200||ammo->magazine||
       profile->speed!=20.f||profile->lifetime!=(kind==13?20.f:3.f)||
       profile->radius!=(kind==13?.2f:.5f)||profile->primary.damage!=80.f||
       profile->primary.damage_kind!=(kind==13?1:5)||profile->liquid_weapon_flags!=(kind==13?0u:0x10600u))return NULL;
    return profile;
}
static int scene_ai_direct_checkpoint_shape(const scene_stream *stream,uint32_t kind,
    const scene_ai_laser_projectile *shot)
{
    const scene_ai_laser_profile *profile=scene_ai_direct_checkpoint_profile(kind);uint32_t i,j,k;
    if(!stream||!shot||!profile||(kind==13?!stream->laser_visual:!stream->sonar_visual))return RF_FORMAT;
    if(shot->flight.active!=1||shot->weapon!=profile->weapon||shot->damage_kind!=profile->primary.damage_kind||
       shot->damage!=combat_enemy_primary_damage(&profile->primary)||shot->liquid_weapon_flags!=profile->liquid_weapon_flags||
       shot->blast_radius!=0||shot->blast_kind||shot->flight.radius!=profile->radius||
       !isfinite(shot->flight.remaining)||shot->flight.remaining<=0||shot->flight.remaining>profile->lifetime||
       (shot->liquid.query_flags!=0x1004u&&(kind==14||shot->liquid.query_flags!=4u)))return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(shot->flight.position[i])||fabsf(shot->flight.position[i])>100000||
           !isfinite(shot->flight.velocity[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(shot->basis[i*3+j]))return RF_FORMAT;
    }
    /* Actual velocity/basis bits survive. Only normalized float roundoff is
     * tolerated: these exact direct families never accelerate or turn. */
    for(i=0;i<3;i++){
        if(fabsf(shot->flight.velocity[i]/profile->speed-shot->basis[6+i])>.001f)return RF_FORMAT;
        for(j=i;j<3;j++){
            double dot=0;for(k=0;k<3;k++)dot+=(double)shot->basis[i*3+k]*shot->basis[j*3+k];
            if(fabs(dot-(i==j?1.0:0.0))>.001)return RF_FORMAT;
        }
        j=(i+1)%3;k=(i+2)%3;
        if(fabsf(shot->basis[j]*shot->basis[3+k]-shot->basis[k]*shot->basis[3+j]-shot->basis[6+i])>.001f)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_ai_direct_checkpoint_capture(const scene_stream *stream,unsigned char *rows,uint32_t *count)
{
    uint32_t kind,i,n=0,uid;int status;
    if(!stream||!count)return RF_RANGE;
    if(scene_ai_direct_checkpoint_busy())return RF_NOT_FOUND;
    for(kind=13;kind<=14;kind++){
        const scene_ai_laser_projectile *shots=kind==13?scene_ai_lasers:scene_ai_sonars;
        uint32_t capacity=kind==13?SCENE_AI_LASER_CAPACITY:SCENE_AI_SONAR_CAPACITY;
        for(i=0;i<capacity;i++){
            const scene_ai_laser_projectile *shot=shots+i;unsigned char *r;
            if(!shot->flight.active)continue;
            status=scene_ai_direct_checkpoint_shape(stream,kind,shot);if(status)return status;
            /* The RFAP6 source helper is independent of Fusion metadata: it
             * proves full NPC registration plus authored nonretired history.
             * Registered corpses and switched weapons retain paid ownership. */
            status=scene_ai_fusion_checkpoint_source(shot->source,&uid);if(status)return status;
            if(!rows){++n;continue;}r=rows+112*n++;
            checkpoint_put(r,kind);checkpoint_put(r+4,i);checkpoint_put(r+8,uid);
            for(uint32_t k=0;k<3;k++){
                checkpoint_put_float(r+16+k*4,shot->flight.position[k]);
                checkpoint_put_float(r+28+k*4,shot->flight.velocity[k]);
            }
            checkpoint_put_float(r+40,shot->flight.radius);checkpoint_put(r+44,shot->liquid.query_flags);
            scene_ai_projectile_put_double(r+48,shot->flight.remaining);checkpoint_put_float(r+56,shot->damage);
            checkpoint_put(r+60,(uint32_t)shot->damage_kind);checkpoint_put(r+64,shot->liquid_weapon_flags);
            for(uint32_t k=0;k<9;k++)checkpoint_put_float(r+72+k*4,shot->basis[k]);
        }
    }
    *count=n;return RF_OK;
}
static int scene_ai_direct_checkpoint_read(const scene_stream *stream,scene_ai_projectile_checkpoint_stage *stage,
    const unsigned char *r)
{
    uint32_t kind=checkpoint_u32(r),slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),source,resolved_uid;
    const scene_ai_laser_profile *profile=scene_ai_direct_checkpoint_profile(kind);
    scene_ai_laser_projectile *shot;uint32_t *sources,*live,capacity;
    if(!stage||!profile||!uid||uid>(uint32_t)INT32_MAX||checkpoint_u32(r+12)||
       checkpoint_u32(r+68)||checkpoint_u32(r+108))return RF_FORMAT;
    capacity=kind==13?SCENE_AI_LASER_CAPACITY:SCENE_AI_SONAR_CAPACITY;
    if(slot>=capacity)return RF_FORMAT;
    shot=(kind==13?stage->lasers:stage->sonars)+slot;
    sources=kind==13?stage->laser_source_uid:stage->sonar_source_uid;
    live=kind==13?&stage->lasers_live:&stage->sonars_live;
    if(shot->flight.active||scene_ai_projectile_source_handle(uid,&source)||
       scene_ai_fusion_checkpoint_source(source,&resolved_uid)||resolved_uid!=uid)return RF_FORMAT;
    shot->source=source;shot->weapon=profile->weapon;shot->damage_kind=(int32_t)checkpoint_u32(r+60);
    shot->liquid_weapon_flags=checkpoint_u32(r+64);shot->damage=checkpoint_float(r+56);
    shot->flight.radius=checkpoint_float(r+40);shot->liquid.query_flags=checkpoint_u32(r+44);
    shot->flight.remaining=scene_ai_projectile_double(r+48);shot->flight.active=1;
    for(uint32_t k=0;k<3;k++){
        shot->flight.position[k]=checkpoint_float(r+16+k*4);shot->flight.velocity[k]=checkpoint_float(r+28+k*4);
    }
    for(uint32_t k=0;k<9;k++)shot->basis[k]=checkpoint_float(r+72+k*4);
    if(scene_ai_direct_checkpoint_shape(stream,kind,shot))return RF_FORMAT;
    sources[slot]=uid;++*live;return RF_OK;
}
static int scene_ai_projectile_checkpoint_direct_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const scene_npc_checkpoint_restore_stage *npcs)
{
    if(!stage)return RF_OK;
    for(uint32_t kind=13;kind<=14;kind++){
        const scene_ai_laser_projectile *shots=kind==13?stage->lasers:stage->sonars;
        const uint32_t *sources=kind==13?stage->laser_source_uid:stage->sonar_source_uid;
        uint32_t capacity=kind==13?SCENE_AI_LASER_CAPACITY:SCENE_AI_SONAR_CAPACITY;
        for(uint32_t i=0;i<capacity;i++)if(shots[i].flight.active){
            const scene_npc_checkpoint_restore_entry *source=NULL;
            if(!npcs||!sources[i])return RF_FORMAT;
            for(uint32_t j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==sources[i]){
                if(source)return RF_FORMAT;source=npcs->entries+j;
            }
            /* Constructor identity is insufficient. The complete candidate
             * must retain this authored source, including corpse eligibility. */
            if(!source||!source->registered||source->saved.retired||!source->handle||source->handle==UINT32_MAX||
               source->handle!=shots[i].source||source->slot>=campaign_npc_body_count||
               (uint32_t)campaign_seeds.records.items[source->slot].record.uid!=sources[i])return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* Called after final storage success and outgoing transient resets. Failed
 * staging never changes these pools. No allocation, callback, debit or launch
 * accompanies publication; old RFAP versions intentionally publish no flights. */
static void scene_ai_projectile_checkpoint_direct_finish(const scene_ai_projectile_checkpoint_stage *stage)
{
    if(!stage)return;
    memcpy(scene_ai_lasers,stage->lasers,sizeof(scene_ai_lasers));
    memcpy(scene_ai_sonars,stage->sonars,sizeof(scene_ai_sonars));
    rf_scene_ai_laser[3]=stage->lasers_live;rf_scene_ai_sonar[3]=stage->sonars_live;
    /* Cosmetic PART history/private visual RNG is not gameplay state. Rebind
     * empty emitters at the resumed endpoint, never replay launch or damage. */
    for(uint32_t i=0;i<SCENE_AI_SONAR_CAPACITY;i++)if(scene_ai_sonars[i].flight.active)
        scene_sonar_visual_bind(i,scene_ai_sonars+i,combat_frame);
}
#endif
