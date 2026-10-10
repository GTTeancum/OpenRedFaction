#ifndef RF_SCENE_ROBOT_DEATH_RUNTIME_INCLUDED
#define RF_SCENE_ROBOT_DEATH_RUNTIME_INCLUDED
/* Include after the shared radial source-aware service, vclip presentation,
 * table parser and existing terminal removal owner. No player blast wrapper,
 * corpse_source_retire, GeoMod, attachment finalizer or broad robot predicate. */
static int scene_robot_death_profiles_open(rf_vpp *tables)
{
    scene_robot_death_profile next[SCENE_ROBOT_DEATH_PROFILE_COUNT]={{0}};rf_vpp_entry entry;void *text=NULL;
    uint32_t i,j,needed=0;int status;
    memset(scene_robot_death_profiles,0,sizeof(scene_robot_death_profiles));
    memset(rf_scene_robot_death,0,sizeof(rf_scene_robot_death));
    if(!tables)return RF_RANGE;
    for(i=0;i<campaign_seeds.records.count;i++)for(j=0;j<SCENE_ROBOT_DEATH_PROFILE_COUNT;j++)
        if(!strcmp(campaign_seeds.records.items[i].record.class_name,scene_robot_death_authored[j].name))needed|=1u<<j;
    if(!needed)return RF_OK;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)goto done;
    if(!entry.size||entry.size>1024*1024){status=RF_RANGE;goto done;}
    text=malloc(entry.size);if(!text){status=RF_IO;goto done;}
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    for(i=0;i<SCENE_ROBOT_DEATH_PROFILE_COUNT;i++)if(needed&(1u<<i)){
        scene_turret_death_profile authored;rf_vclip_definition clip;
        uint32_t found=0;scene_robot_death_profile *p=next+i;
        p->clip=p->death_group=p->foley_group=-1;
        status=scene_turret_death_read(text,entry.size,scene_robot_death_authored[i].name,&authored);if(status)goto done;
        if(strcmp(authored.clip_name,scene_robot_death_authored[i].clip_name)||
           strcmp(authored.sound,scene_robot_death_authored[i].death_sound)||
           !isfinite(authored.radius)||authored.radius!=scene_robot_death_authored[i].radius){status=RF_FORMAT;goto done;}
        for(j=0;j<3;j++)if(!isfinite(authored.offset[j])){status=RF_FORMAT;goto done;}
        for(j=0;j<campaign_seeds.records.count;j++)if(!strcmp(campaign_seeds.records.items[j].record.class_name,scene_robot_death_authored[i].name)){
            uint32_t cls=campaign_seeds.items[j].class_index;
            if(cls>=campaign_seeds.class_count||(found&&p->class_index!=cls)||
               campaign_seeds.classes[cls].corpse.model[0]){status=RF_FORMAT;goto done;}
            p->class_index=cls;found=1;
        }
        if(!found){status=RF_FORMAT;goto done;}
        p->clip=rf_vclip_name_lookup(campaign_clutter_catalogs.names.vclips,authored.clip_name);
        if(p->clip<0||p->clip>=64){status=RF_NOT_FOUND;goto done;}
        status=rf_vclip_definition_load(tables,authored.clip_name,65536,&clip);if(status)goto done;
        if(!isfinite(clip.damage)||clip.damage!=scene_robot_death_authored[i].clip_damage){status=RF_FORMAT;goto done;}
        /* Original4364da/4364e7: clip damage * authored radius * multiplier1.
         * Spike's yellboom_metal has no Damage field: preserve zero all the
         * way into the existing shared radial service's nonpositive no-op. */
        p->radius=authored.radius;p->damage=(float)((double)clip.damage*p->radius);
        if(!isfinite(p->damage)){status=RF_RANGE;goto done;}
        memcpy(p->offset,authored.offset,12);memcpy(p->clip_name,authored.clip_name,64);
        /* Name resolution has no PCM/visual admission dependency. Missing
         * optional audio groups remain silent; gameplay metadata still lives. */
        (void)rf_foley_find(&campaign_foley,authored.sound,&p->death_group);
        if(clip.has_foley)(void)rf_foley_find(&campaign_foley,clip.foley,&p->foley_group);
        p->ready=1;
    }
    memcpy(scene_robot_death_profiles,next,sizeof(next));
 done:
    free(text);rf_scene_robot_death[13]=(uint32_t)status;return status;
}
static void scene_robot_death_visual_open(rf_vpp *tables,rf_vpp *maps,uint32_t map_count)
{
    uint32_t i;for(i=0;i<SCENE_ROBOT_DEATH_PROFILE_COUNT;i++)if(scene_robot_death_profiles[i].ready){
        const scene_robot_death_profile *p=scene_robot_death_profiles+i;
        int status=scene_script_explode_effects_open(tables,maps,map_count,(uint32_t)p->clip,p->clip_name);
        if(status)++rf_scene_robot_death[6]; /* Shared 512KiB/four-clip cosmetic budget. */
    }
}
/* Same allocated row plus epoch, authored UID/class, persistence key and full
 * retained damage handle qualify either the still-live registry owner or an
 * intervening Remove_Object tombstone. A paid death is not cancelled simply
 * because that event unregistered its source before this outer boundary. */
static int scene_robot_death_retire(uint32_t slot,const scene_robot_death_receipt *r,uint64_t epoch)
{
    campaign_npc_body *o;rf_campaign_object_record *key;rf_campaign_weapon_drop drop;
    uint32_t persistent;int status;
    if(epoch!=scene_robot_death_epoch)return RF_NOT_FOUND;
    if(!r||!campaign_npc_bodies||slot>=campaign_npc_body_count||slot>=campaign_seeds.records.count||
       r->profile>=SCENE_ROBOT_DEATH_PROFILE_COUNT||scene_robot_death_profile_at(slot)!=(int32_t)r->profile||
       campaign_seeds.items[slot].class_index!=r->class_index||
       (uint32_t)campaign_seeds.records.items[slot].record.uid!=r->uid)return RF_FORMAT;
    o=campaign_npc_bodies+slot;persistent=r->persistence;
    if(o->robot_death.state!=2||o->robot_death.handle!=r->handle||o->robot_death.sequence!=r->sequence||
       o->damage.effects.handle!=r->handle||!o->persistence_registered||o->persistence_slot!=persistent||
       persistent>=rf_scene_defeated_actors.count||persistent>=RF_CAMPAIGN_ACTOR_SLOTS||
       !isfinite(r->health)||r->health>0||!isfinite(o->damage.effects.health)||o->damage.effects.health>0||
       !(o->view.flags_810&1u)||o->death.action_824!=-1||o->script_once.count>16||
       campaign_seeds.classes[r->class_index].corpse.model[0]||
       !campaign_model_owners||slot>=campaign_model_owner_count||campaign_model_owners[slot].owned)return RF_FORMAT;
    key=rf_scene_defeated_actors.items+persistent;
    if(key->uid!=r->uid||key->level>=rf_scene_defeated_actors.level_count||key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
       !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level))return RF_FORMAT;
    /* These production attacks have no embedded firing/effect tokens. Keep
     * future unsupported owners visible rather than silently dropping them. */
    for(uint32_t i=0;i<64;i++)if(o->firing.active[i])return RF_NOT_FOUND;
    if(o->firing.sound_81c!=-1||o->firing.sound_820!=-1||o->firing.effect_13d4!=-1)return RF_NOT_FOUND;
    drop=rf_scene_defeated_actors.drops[persistent];
    if(o->registration.view){
        if(key->retired||!scene_robot_death_identity(slot,r->handle)||!o->body.allocated_bytes)return RF_FORMAT;
        status=campaign_remove_object(NULL,r->handle);if(status)return status;
        if(epoch!=scene_robot_death_epoch)return RF_NOT_FOUND;
        /* Never use the pre-callback pointer. The allocated actor row survives
         * normal removal; a timeline replacement is caught by epoch above. */
        if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_FORMAT;
        o=campaign_npc_bodies+slot;key=rf_scene_defeated_actors.items+persistent;
        if(o->registration.view||o->body.allocated_bytes||!key->retired||key->uid!=r->uid||
           o->damage.effects.handle!=r->handle||o->robot_death.sequence!=r->sequence)return RF_FORMAT;
        ++rf_scene_robot_death[2];
    }else{
        if(!key->retired||o->body.allocated_bytes||o->view.handle!=-1||
           rf_object_registry_lookup(&campaign_registry,r->handle)||
           rf_entity_lookup(&campaign_entities,(int32_t)r->handle))return RF_FORMAT;
        ++rf_scene_robot_death[3];
    }
    o->damage.effects.health=r->health;
    rf_scene_defeated_actors.vitals[persistent].valid=0;
    rf_scene_defeated_actors.vitals[persistent].health=r->health;
    rf_scene_defeated_actors.vitals[persistent].armor=o->damage.effects.armor;
    rf_scene_defeated_actors.mission[persistent].affiliation=o->damage.effects.affiliation;
    rf_scene_defeated_actors.mission[persistent].flags=o->object_flags&0x4004u;
    rf_scene_defeated_actors.drops[persistent]=drop;
    return RF_OK;
}
static int scene_robot_death_tick(scene_stream *stream,uint32_t frame)
{
    uint64_t epoch;uint32_t budget;int status=RF_OK;
    if(!stream)return RF_RANGE;if(scene_robot_death_draining)return RF_OK;
    if(scene_robot_death_fault)return (int)scene_robot_death_fault;
    epoch=scene_robot_death_epoch;scene_robot_death_draining=1;++rf_scene_robot_death[14];
    /* Bound chains without losing the remainder. Subsequent outer ticks drain
     * FIFO receipts, and saving stays blocked until all gameplay is consumed. */
    for(budget=0;budget<32;budget++){
        scene_robot_death_receipt receipt;uint32_t slot=UINT32_MAX,i,before;
        uint64_t first=UINT64_MAX;
        if(epoch!=scene_robot_death_epoch)break;
        if(campaign_npc_body_count&&!campaign_npc_bodies){status=RF_RANGE;break;}
        for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].robot_death.state==1&&
           (slot==UINT32_MAX||campaign_npc_bodies[i].robot_death.sequence<first)){first=campaign_npc_bodies[i].robot_death.sequence;slot=i;}
        if(slot==UINT32_MAX)break;
        receipt=campaign_npc_bodies[slot].robot_death;
        campaign_npc_bodies[slot].robot_death.state=2; /* Before any callback. */
        ++rf_scene_robot_death[1];if(rf_scene_robot_death[10])--rf_scene_robot_death[10];
        rf_scene_robot_death[11]=receipt.uid;rf_scene_robot_death[12]=receipt.handle;
        /*418f80 marks removal before419420. No borrowed owner survives this. */
        status=scene_robot_death_retire(slot,&receipt,epoch);
        if(epoch!=scene_robot_death_epoch){status=RF_OK;break;}if(status)break;
        before=rf_scene_script_explode_visual[0];
        status=scene_script_explode_effects_start(stream,receipt.position,frame,receipt.radius,(uint32_t)receipt.clip);
        if(epoch!=scene_robot_death_epoch){status=RF_OK;break;}
        if(status||rf_scene_script_explode_visual[0]==before)++rf_scene_robot_death[6];
        else ++rf_scene_robot_death[5];
        /*436490 requests the effect first. Optional presentation failure does
         * not suppress the authored source-1/kind3 radial request. Spike's
         * zero amount retains the shared no-damage/no-push early return. */
        scene_robot_death_foley(receipt.foley_group,receipt.position);
        if(epoch!=scene_robot_death_epoch){status=RF_OK;break;}
        ++rf_scene_robot_death[4];
        status=scene_explosion_blast_source_guarded(stream,frame,receipt.position,
            receipt.damage,receipt.radius,UINT32_MAX,3,&scene_robot_death_epoch);
        if(epoch!=scene_robot_death_epoch){status=RF_OK;break;}if(status)break;
    }
    scene_robot_death_draining=0;
    if(status&&epoch==scene_robot_death_epoch)scene_robot_death_fault=(uint32_t)status;
    rf_scene_robot_death[13]=(uint32_t)status;return status;
}
#endif
