#ifndef RF_SCENE_WORLD_CHECKPOINT_IDENTITY_INCLUDED
#define RF_SCENE_WORLD_CHECKPOINT_IDENTITY_INCLUDED
/* Include after checkpoint_sha helpers and campaign registry declarations.
 * Versioned player pseudo-UID is reserved globally in this save profile;
 * authored objects/items may not use it. Keys are aliases to controller UIDs,
 * never additional canonical handle identities. */
#define SCENE_WORLD_CHECKPOINT_PLAYER_UID (UINT32_MAX-1u)
static int scene_world_checkpoint_uid_add(rf_level_uid_object *rows,uint32_t *count,
    uint32_t uid,uint32_t handle,uint32_t flags,const void *expected,uint32_t absent)
{
    uint32_t i;void *registered;
    if(uid==UINT32_MAX||uid==SCENE_WORLD_CHECKPOINT_PLAYER_UID)return RF_FORMAT;
    registered=handle==UINT32_MAX?NULL:rf_object_registry_lookup(&campaign_registry,handle);
    if(absent){if(registered)return RF_FORMAT;handle=UINT32_MAX;}
    else if(!expected||!handle||handle==UINT32_MAX||registered!=expected)return RF_FORMAT;
    for(i=0;i<*count;i++)if(rows[i].uid==uid||(handle!=UINT32_MAX&&rows[i].handle==handle))return RF_FORMAT;
    rows[*count]=(rf_level_uid_object){uid,handle,flags};(*count)++;return RF_OK;
}
static int scene_world_checkpoint_uid_map_open(const scene_stream *scene,rf_level_uid_object **out,uint32_t *count,uint32_t budget)
{
    rf_level_uid_object *rows;uint64_t capacity;uint32_t n=0,i,j;int status;
    if(!scene||!out||*out||!count)return RF_RANGE;
    capacity=(uint64_t)campaign_events.count+campaign_triggers.count+campaign_group_registration.count+
        campaign_mover_count+campaign_npc_body_count+campaign_clutter_records.count+scene->pickups.count+1;
    if(capacity>UINT32_MAX/sizeof(*rows)||capacity*sizeof(*rows)>budget)return RF_RANGE;
    if((campaign_events.count&&!campaign_events.items)||(campaign_triggers.count&&!campaign_triggers.items)||
       (campaign_group_registration.count&&(!campaign_group_registration.objects||!campaign_group_registration.controllers))||
       (campaign_mover_count&&(!campaign_mover_objects||!campaign_mover_wrappers))||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||campaign_seeds.records.count<campaign_npc_body_count))||
       (campaign_clutter_records.count&&!campaign_clutter_records.items)||(scene->pickups.count&&!scene->pickups.items))return RF_RANGE;
    rows=malloc((size_t)capacity*sizeof(*rows));if(!rows)return RF_IO;
#define WORLD_UID(uid,handle,flags,owner,absent) do{status=scene_world_checkpoint_uid_add(rows,&n,uid,handle,flags,owner,absent);if(status)goto fail;}while(0)
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *e=campaign_events.items+i;if(!e->authored||e->retired>1){status=RF_FORMAT;goto fail;}
        WORLD_UID(e->authored->record.uid,e->handle,0,e,e->retired);
    }
    for(i=0;i<campaign_triggers.count;i++){
        const rf_runtime_trigger *t=campaign_triggers.items+i;if(!t->authored){status=RF_FORMAT;goto fail;}
        WORLD_UID(t->authored->record.uid,t->handle,0,t,0);
    }
    for(i=0;i<campaign_group_registration.count;i++){
        const rf_level_uid_object *o=campaign_group_registration.objects+i;
        if(o->handle!=campaign_group_registration.controllers[i].handle){status=RF_FORMAT;goto fail;}
        WORLD_UID(o->uid,o->handle,o->flags,campaign_group_registration.controllers+i,0);
    }
    for(i=0;i<campaign_mover_count;i++){
        const rf_level_uid_object *o=campaign_mover_objects+i;
        if(o->handle!=campaign_mover_wrappers[i].handle){status=RF_FORMAT;goto fail;}
        WORLD_UID(o->uid,o->handle,o->flags,campaign_mover_wrappers+i,0);
    }
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.view&&(o->registration.view!=&o->view||
           rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)!=&o->view)){status=RF_FORMAT;goto fail;}
        if(!o->registration.view&&o->persistence_registered){
            if(o->persistence_slot>=rf_scene_defeated_actors.count||!rf_scene_defeated_actors.items[o->persistence_slot].retired){status=RF_FORMAT;goto fail;}
        }
        WORLD_UID(campaign_seeds.records.items[i].record.uid,o->registration.view?o->registration.handle:UINT32_MAX,
            o->object_flags,&o->registration,!o->registration.view);
    }
    for(i=0;i<campaign_clutter_records.count;i++){
        const rf_clutter_base_owner *o=campaign_clutter_bodies?campaign_clutter_bodies[i]:NULL;
        WORLD_UID(campaign_clutter_records.items[i].uid,o?o->state.handle:UINT32_MAX,o?o->state.flags:2u,o?&o->state:NULL,!o);
    }
    /* Authored pickups have no object registration in the current scene.
     * Reserve/check their UIDs, but do not invent event reference handles. */
    for(i=0;i<scene->pickups.count;i++)WORLD_UID(scene->pickups.items[i].uid,UINT32_MAX,0,NULL,1);
#undef WORLD_UID
    /* Keys are alternative authored addresses for an existing controller.
     * They must not collide with a canonical object's UID or player tag. */
    if(campaign_group_registration.key_count&&!campaign_group_registration.keys){status=RF_RANGE;goto fail;}
    for(i=0;i<campaign_group_registration.key_count;i++){
        const rf_level_uid_key *key=campaign_group_registration.keys+i;uint32_t owner=0;
        if(key->uid==UINT32_MAX||key->uid==SCENE_WORLD_CHECKPOINT_PLAYER_UID){status=RF_FORMAT;goto fail;}
        /* The first key IS the controller's canonical UID in level.c1784. */
        for(j=0;j<n;j++)if(rows[j].uid==key->uid&&rows[j].handle!=key->handle){status=RF_FORMAT;goto fail;}
        for(j=0;j<i;j++)if(campaign_group_registration.keys[j].uid==key->uid){status=RF_FORMAT;goto fail;}
        for(j=0;j<campaign_group_registration.count;j++)if(campaign_group_registration.controllers[j].handle==key->handle)owner++;
        if(owner!=1){status=RF_FORMAT;goto fail;}
    }
    /* Player has no authored object UID. Existing zero/-1 handle sentinels
     * remain codec sentinels and must not be used for this special owner. */
    if(!campaign_player_object.view||campaign_player_object.view!=&campaign_player_view||!campaign_player_object.handle||
       campaign_player_object.handle==UINT32_MAX||rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)!=&campaign_player_object){status=RF_FORMAT;goto fail;}
    for(i=0;i<n;i++)if(rows[i].handle==campaign_player_object.handle){status=RF_FORMAT;goto fail;}
    rows[n++]=(rf_level_uid_object){SCENE_WORLD_CHECKPOINT_PLAYER_UID,campaign_player_object.handle,campaign_player_view.flags_7c};
    for(i=0,j=0;i<n;i++)if(rows[i].handle!=UINT32_MAX)rows[j++]=rows[i];
    if(j>RF_OBJECT_CAPACITY){status=RF_RANGE;goto fail;}*out=rows;*count=j;return RF_OK;
fail:free(rows);return status;
}
/* Boot-only identity. Full original RFL entry and full validated tables.vpp
 * bytes bind sections, class/weapon definitions and catalog order; the runtime
 * weapon catalog hash additionally binds its loader policy. This does NOT hash
 * external model/motion/texture/audio payloads. A new resource manifest/version
 * is required before claiming cross-install asset compatibility. No DEV data. */
static int scene_world_checkpoint_source_identity(const rf_level *level,const char *tables_path,
    uint32_t weapon_catalog_hash,unsigned char out[32])
{
    checkpoint_sha hash;rf_vpp tables={0};unsigned char scratch[4096],result[32];
    uint32_t offset,n;long position;int status=RF_OK;
    if(!level||!level->archive||!level->archive->stream||!tables_path||!out||
       !memchr(level->entry.name,0,sizeof(level->entry.name)))return RF_RANGE;
    position=ftell(level->archive->stream);if(position<0)return RF_IO;
    status=rf_vpp_open(&tables,tables_path);if(status)return status;
    checkpoint_sha_init(&hash);checkpoint_sha_add(&hash,"RF ordinary source v1",22);
    checkpoint_sha_word(&hash,weapon_catalog_hash);
    checkpoint_sha_add(&hash,level->entry.name,(uint32_t)strlen(level->entry.name)+1);checkpoint_sha_word(&hash,level->entry.size);
    for(offset=0;offset<level->entry.size;offset+=n){
        n=level->entry.size-offset;if(n>sizeof(scratch))n=sizeof(scratch);
        status=rf_vpp_read(level->archive,&level->entry,offset,scratch,n);if(status)goto done;
        checkpoint_sha_add(&hash,scratch,n);
    }
    checkpoint_sha_word(&hash,tables.length);if(fseek(tables.stream,0,SEEK_SET)){status=RF_IO;goto done;}
    for(offset=0;offset<tables.length;offset+=n){
        n=tables.length-offset;if(n>sizeof(scratch))n=sizeof(scratch);
        if(fread(scratch,1,n,tables.stream)!=n){status=RF_IO;goto done;}checkpoint_sha_add(&hash,scratch,n);
    }
    checkpoint_sha_end(&hash,result);
done:rf_vpp_close(&tables);if(fseek(level->archive->stream,position,SEEK_SET))status=RF_IO;
    if(!status)memcpy(out,result,32);return status;
}
#endif
