/* Private RFCP scene adapter; included after RFDS restore and before transport. */
enum {SCENE_PLAYER_CHECKPOINT_PROFILE=1,SCENE_PLAYER_CHECKPOINT_PREFIX=32+RF_PLAYER_CHECKPOINT_BYTES};
static uint32_t scene_checkpoint_profile(const scene_stream *s)
{return s->terrain_authored?(s->terrain_source_count>1?RF_COMPOSED_PROFILE_AUTHORED_COLLECTION:RF_COMPOSED_PROFILE_AUTHORED):SCENE_PLAYER_CHECKPOINT_PROFILE;}
static int scene_checkpoint_player_catalog(rf_player_checkpoint_catalog *c)
{
    uint32_t i;memset(c,0,sizeof(*c));c->hash=rf_scene_weapon_supply[3];c->count=campaign_weapon_supply.names.count;
    if(!c->count||c->count>64)return RF_FORMAT;
    memcpy(c->weapons,campaign_weapon_supply.definitions,sizeof(c->weapons));
    c->health_capacity=campaign_player_damage.state.effects.class_health;c->armor_capacity=campaign_player_damage.state.effects.class_armor;
    for(i=0;i<SCENE_WEAPON_SLOTS;i++){
        if(i==9)continue; /* Detonator shares the base remote item. */
        int32_t id=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[i]);const rf_weapon_acquire_definition *d;
        if(id<0||(uint32_t)id>=c->count)return RF_FORMAT;c->supported[id]=1;d=c->weapons+id;
        if(d->ammo_type<0||d->ammo_type>=32||d->capacity<0)return RF_FORMAT;
        if(d->capacity>c->reserve_capacity[d->ammo_type])c->reserve_capacity[d->ammo_type]=d->capacity;
    }
    return RF_OK;
}
static int scene_checkpoint_player_scope(const scene_stream *s,uint32_t live)
{
    uint32_t i;
    if(!rf_scene_dev_room_enabled||rf_scene_water_test_enabled||!campaign_spawn||!s->terrain||
       campaign_movers.count||campaign_npc_body_count||
       !scene_actor_body.allocated_bytes||!scene_actor_body.spheres.count||scene_actor_body.spheres.count>8)return RF_RANGE;
    if(s->terrain_authored) {
        if(strcmp(campaign_current_level,"ctf06.rfl") || !s->checkpoint_clutter ||
           scene_authored_clutter_baseline_check(s->terrain_source_count>1?s->terrain_sources[0].authored->source_identity:s->terrain_authored->source_identity,0,s->checkpoint_clutter))return RF_RANGE;
    } else if(strcmp(campaign_current_level,"glass_house.rfl") || rf_scene_clutter_bodies[1])return RF_RANGE;
    if(!live)return RF_OK;
    if(campaign_player_damage.state.effects.health<=0||campaign_crouched||(rf_scene_actor_stance_flags&0x400u)||
       campaign_climb.region||campaign_player_view.linked_handle!=-1||campaign_support_handle||rf_scene_actor_landing[1]!=1||
       combat_trigger.cooldown||combat_trigger.remaining||combat_trigger.delay||combat_trigger.held||rf_scene_combat[6]||
       riot_charge_remainder||rf_scene_rockets[3]||scene_flame_input_pending())return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(scene_actor_body.state.velocity[i])||fabsf(scene_actor_body.state.velocity[i])>.001f||
        actor_look.state.command[i]!=0||actor_look.state.angular_velocity[i]!=0)return RF_RANGE;
    if(actor_look.state.pending_pitch!=0||actor_look.state.pending_yaw!=0)return RF_RANGE;
    return RF_OK;
}
static void scene_checkpoint_placement(scene_stream *s,const rf_player_checkpoint *player,const float body[9],rf_checkpoint_placement *p)
{
    memset(p,0,sizeof(*p));p->world=s->collision;p->replaced_room=s->terrain_collision.room;
    /* Dry swimming transition retains liquid-query bit 0x1000. Placement tests
     * solid clearance only, matching rf_physics_ground_probe_prepare; it does
     * not treat a liquid surface as an enclosing solid. Other flags survive. */
    p->query_flags=(scene_actor_body.state.state_124&~0x1000u)|4u;p->spheres=scene_actor_body.spheres.items;p->count=scene_actor_body.spheres.count;
    memcpy(p->position,player->position,12);memcpy(p->basis,body,36);
}
/* Transport RF_NOT_FOUND means absent files. A present RFCP whose physical
 * candidate has no valid fit/support is malformed, never an empty save store. */
static int scene_checkpoint_player_semantic_status(int status)
{return status==RF_NOT_FOUND?RF_FORMAT:status;}
static int scene_checkpoint_dispatch_validate_once(const void *data,uint32_t bytes,void *context)
{
    scene_stream *s=context;rf_player_checkpoint_catalog catalog;rf_composed_checkpoint candidate;rf_composed_checkpoint_v2 composed;rf_checkpoint_placement placement;
    float body[9];int status;
    if(!rf_scene_player_checkpoint_enabled)return scene_checkpoint_validate(data,bytes,s);
    status=scene_checkpoint_player_scope(s,0);if(status)return status;
    status=scene_checkpoint_player_catalog(&catalog);if(status)return status;
    status=rf_composed_checkpoint_preflight_v2(data,bytes,scene_checkpoint_profile(s),&catalog,
        scene_remote_checkpoint_level_hash(),scene_remote_checkpoint_catalog_hash(),&composed);if(status)return status;
    candidate=composed.base;
    status=rf_player_checkpoint_validate(&candidate.player,&catalog,body,NULL);if(status)return status;
    scene_checkpoint_placement(s,&candidate.player,body,&placement);
    if(s->terrain_authored && s->terrain_source_count>1) {
        scene_authored_collection_stage *stage=NULL;
        status=scene_authored_collection_stage_prepare(s,candidate.rfds,candidate.rfds_bytes,&stage);
        if(!status)status=scene_authored_collection_stage_player(stage,&placement);
        scene_authored_collection_stage_discard(&stage);
    } else if(s->terrain_authored) {
        scene_authored_checkpoint_stage *stage=NULL;
        status=scene_authored_checkpoint_stage_prepare(s,candidate.rfds,candidate.rfds_bytes,&placement,&stage);
        scene_authored_checkpoint_stage_discard(&stage);
    } else status=scene_checkpoint_validate_player(candidate.rfds,candidate.rfds_bytes,s,&placement);
    if(!status)status=scene_remote_checkpoint_preflight(s,composed.remote,composed.remote_bytes);
    scene_remote_checkpoint_discard();
    return scene_checkpoint_player_semantic_status(status);
}
static int scene_checkpoint_dispatch_validate(const void *data,uint32_t bytes,void *context)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    if(rf_scene_player_checkpoint_enabled && getenv("RF_DEV_GEOMOD_CHECKPOINT_VALIDATE_AUDIT")){
        unsigned char before[32],after[32];int status,repeat,check;
        check=scene_checkpoint_audit_hash(context,data,bytes,before);if(check)return check;
        status=scene_checkpoint_dispatch_validate_once(data,bytes,context);
        check=scene_checkpoint_audit_hash(context,data,bytes,after);if(check)return check;if(memcmp(before,after,32))return RF_FORMAT;
        repeat=scene_checkpoint_dispatch_validate_once(data,bytes,context);
        check=scene_checkpoint_audit_hash(context,data,bytes,after);if(check)return check;if(repeat!=status||memcmp(before,after,32))return RF_FORMAT;
        printf("PLAYER_CHECKPOINT_VALIDATE_AUDIT PASS %d %u\n",status,bytes);return status;
    }
#endif
    return scene_checkpoint_dispatch_validate_once(data,bytes,context);
}
static int scene_checkpoint_dispatch_restore(scene_stream *s,unsigned char *data,uint32_t bytes)
{
    rf_player_checkpoint_catalog catalog;rf_composed_checkpoint candidate;rf_composed_checkpoint_v2 composed;rf_physics_body_state body;float basis[9],eye[9];int status;
    if(!rf_scene_player_checkpoint_enabled)return scene_checkpoint_restore(s,data,bytes);
    status=scene_checkpoint_dispatch_validate(data,bytes,s);if(status)return status;
    status=scene_checkpoint_player_catalog(&catalog);if(status)return status;
    status=rf_composed_checkpoint_preflight_v2(data,bytes,scene_checkpoint_profile(s),&catalog,
        scene_remote_checkpoint_level_hash(),scene_remote_checkpoint_catalog_hash(),&composed);if(status)return status;
    candidate=composed.base;
    status=rf_player_checkpoint_validate(&candidate.player,&catalog,basis,eye);if(status)return status;
    body=scene_actor_body.state;memcpy(body.position,candidate.player.position,12);memcpy(body.next_position,body.position,12);
    memcpy(body.orientation,basis,36);memcpy(body.next_orientation,basis,36);memcpy(body.velocity,candidate.player.velocity,12);
    status=rf_physics_spheres_bounds(scene_actor_body.spheres.items,scene_actor_body.spheres.count,body.position,&body.bounds);if(status)return status;
    status=rf_physics_tensor_world(body.local_tensor,basis,body.world_tensor);if(status)return status;
    status=scene_remote_checkpoint_preflight(s,composed.remote,composed.remote_bytes);if(status)return status;
    /* Existing RFDS semantic validator and candidate body fit have succeeded.
     * Prepare all fallible player work before RFDS publication; only assignments follow. */
    if(s->terrain_authored && s->terrain_source_count>1) {
        scene_authored_collection_stage *stage=NULL;rf_checkpoint_placement placement;
        scene_checkpoint_placement(s,&candidate.player,basis,&placement);
        status=scene_authored_collection_stage_prepare(s,candidate.rfds,candidate.rfds_bytes,&stage);
        if(!status)status=scene_authored_collection_stage_commit(stage,&placement);
        scene_authored_collection_stage_discard(&stage);
    } else if(s->terrain_authored) {
        scene_authored_checkpoint_stage *stage=NULL;rf_checkpoint_placement placement;
        scene_checkpoint_placement(s,&candidate.player,basis,&placement);
        status=scene_authored_checkpoint_stage_prepare(s,candidate.rfds,candidate.rfds_bytes,&placement,&stage);
        if(!status)status=scene_authored_checkpoint_stage_commit(stage);
        scene_authored_checkpoint_stage_discard(&stage);
    } else status=scene_checkpoint_restore(s,(unsigned char *)candidate.rfds,candidate.rfds_bytes);
    if(status){scene_remote_checkpoint_discard();return status;}
    scene_remote_checkpoint_publish();
    scene_actor_body.state=body;s->player_checkpoint_value=candidate.player;s->player_checkpoint_look=1;
    memcpy(s->actor_spawn,body.position,12);memcpy(campaign_position,body.position,12);memcpy(campaign_orientation,eye,36);
    campaign_player_import=candidate.player.player;campaign_import_pending=1;campaign_item_pending_count=0;
    rf_scene_player_checkpoint_state[1]=1;rf_scene_player_checkpoint_state[5]=candidate.rfds_bytes;
    rf_scene_player_checkpoint_state[6]=scene_actor_body.spheres.count;
    printf("PLAYER_CHECKPOINT_LOAD %u %u %u\n",bytes,candidate.rfds_bytes,scene_actor_body.spheres.count);return RF_OK;
}
/* RFCP profile 1 admits only settled standing on static world support.
 * Reconstruct that locomotion state after ordinary scene initialization; using
 * the new-game falling state would integrate one gravity step before landing. */
static void scene_checkpoint_player_locomotion(void)
{
    if(!rf_scene_player_checkpoint_enabled || !rf_scene_player_checkpoint_state[1])return;
    rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=0;
    scene_actor_body.state.flags&=~(0x200000u|0x400000u);
    /* Restore already supplied the saved residual velocity (zero for RFPL v1). */
    campaign_support_handle=0;memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));
}
static int scene_checkpoint_player_capture(scene_stream *s,rf_player_checkpoint *player,rf_player_checkpoint_catalog *catalog)
{
    rf_geomod_terrain_view view;rf_checkpoint_placement placement;float body[9];int status;
    status=scene_checkpoint_player_scope(s,1);if(status){
        printf("PLAYER_CHECKPOINT_REJECT scope %d dev%u water%u spawn%u movers%u npcs%u clutter%u body%u spheres%u health%g crouch%u stance%u climb%u link%d support%u landing%u cooldown%u remaining%u delay%u held%u reload%u charge%u rockets%u\n",status,
            rf_scene_dev_room_enabled,rf_scene_water_test_enabled,campaign_spawn,campaign_movers.count,campaign_npc_body_count,rf_scene_clutter_bodies[1],scene_actor_body.allocated_bytes,scene_actor_body.spheres.count,
            (double)campaign_player_damage.state.effects.health,campaign_crouched,rf_scene_actor_stance_flags,campaign_climb.region!=NULL,campaign_player_view.linked_handle,campaign_support_handle,rf_scene_actor_landing[1],combat_trigger.cooldown,combat_trigger.remaining,combat_trigger.delay,combat_trigger.held,rf_scene_combat[6],riot_charge_remainder,rf_scene_rockets[3]);
        printf("PLAYER_CHECKPOINT_MOTION velocity %.9g %.9g %.9g command %.9g %.9g %.9g angular %.9g %.9g %.9g pending %.9g %.9g\n",
            (double)scene_actor_body.state.velocity[0],(double)scene_actor_body.state.velocity[1],(double)scene_actor_body.state.velocity[2],
            (double)actor_look.state.command[0],(double)actor_look.state.command[1],(double)actor_look.state.command[2],
            (double)actor_look.state.angular_velocity[0],(double)actor_look.state.angular_velocity[1],(double)actor_look.state.angular_velocity[2],(double)actor_look.state.pending_pitch,(double)actor_look.state.pending_yaw);
        return status;
    }
    status=scene_checkpoint_player_catalog(catalog);if(status){printf("PLAYER_CHECKPOINT_REJECT catalog %d\n",status);return status;}
    memset(player,0,sizeof(*player));player->player.inventory=campaign_player_inventory;
    player->player.health=campaign_player_damage.state.effects.health;player->player.armor=campaign_player_damage.state.effects.armor;
    player->player.weapon=campaign_explicit_unarmed?UINT32_MAX:(uint32_t)campaign_selected_weapon();
    if(!campaign_player_inventory.owned[campaign_selected_weapon()])player->player.weapon=UINT32_MAX;
    player->player.catalog_hash=rf_scene_weapon_supply[3];memcpy(player->position,scene_actor_body.state.position,12);
    memcpy(player->body_angles,actor_look.state.body_angles,12);memcpy(player->eye_angles,actor_look.state.eye_angles,12);
    memcpy(player->velocity,scene_actor_body.state.velocity,12);
    status=rf_player_checkpoint_validate(player,catalog,body,NULL);if(status){printf("PLAYER_CHECKPOINT_REJECT player %d health %.9g/%.9g armor %.9g/%.9g\n",status,(double)player->player.health,(double)catalog->health_capacity,(double)player->player.armor,(double)catalog->armor_capacity);return status;}
    scene_checkpoint_placement(s,player,body,&placement);status=rf_geomod_terrain_get(s->terrain,&view);if(status)return status;
    if(s->terrain_authored) {
        rf_collision_composition_view composed;
        status=scene_terrain_publication_view(s,&view);if(status)return status;
        status=rf_collision_composition_get(s->terrain_publication->composition,&composed);if(status)return status;
        view.faces=composed.tree->faces;view.tree=composed.tree;view.mesh.face_count=composed.count;
    }
    {rf_checkpoint_placement_result fit;rf_geomod_player_support_context support={s->detached_pieces,&placement};
     scene_collection_support_context collection_support={s,&placement};
     status=rf_checkpoint_standing_check_with_support(&view,&placement,scene_step_seconds,rf_scene_actor_movement_values.speed,
         s->terrain_source_count>1?scene_collection_player_support:rf_geomod_piece_registry_player_support,
         s->terrain_source_count>1?(void *)&collection_support:(void *)&support,&fit);if(status){printf("PLAYER_CHECKPOINT_REJECT placement %d sphere%u reason%u retries%u\n",status,status==RF_NOT_FOUND?fit.sphere:UINT32_MAX,status==RF_NOT_FOUND?fit.reason:0,status==RF_NOT_FOUND?fit.retries:0);return status;}}

    status=s->terrain_source_count>1?scene_collection_player_placement(s,&placement):rf_geomod_piece_registry_placement_check(s->detached_pieces,&placement);if(status){
        uint32_t source_slot;
        for(source_slot=0;source_slot<(s->terrain_source_count?s->terrain_source_count:1);source_slot++) {
        rf_geomod_piece_registry *registry=scene_detached_source_registry(s,source_slot);
        uint32_t b,n,q,k,j;double minimum=1e30;uint32_t source_index=UINT32_MAX,target_index=UINT32_MAX;
        for(b=0;b<rf_geomod_piece_registry_count(registry);b++) {
            rf_geomod_piece_batch *batch=NULL;uint32_t piece_index;
            if(rf_geomod_piece_registry_get(registry,b,&batch))continue;
            for(piece_index=0;piece_index<rf_geomod_piece_batch_count(batch);piece_index++) {
                rf_geomod_owned_piece piece;rf_physics_body *target=NULL;
                if(!rf_geomod_piece_batch_alive(batch,piece_index) || rf_geomod_piece_batch_get(batch,piece_index,&piece,&target))continue;
                if(!(target->state.bounds.radius>.5f && target->state.bounds.radius<=1))continue;
                for(n=0;n<placement.count;n++)for(q=0;q<target->spheres.count;q++) {
                    double distance=0;
                    for(k=0;k<3;k++) {
                        double delta=(double)placement.position[k]-target->state.position[k];
                        for(j=0;j<3;j++)delta+=(double)placement.spheres[n].center[j]*placement.basis[j*3+k]-(double)target->spheres.items[q].center[j]*target->state.orientation[j*3+k];
                        distance+=delta*delta;
                    }
                    distance=sqrt(distance)-placement.spheres[n].radius-target->spheres.items[q].radius;
                    if(distance<minimum){minimum=distance;source_index=n;target_index=q;}
                }
            }
        }
        printf("PLAYER_CHECKPOINT_REJECT rubble %d clearance %.9g source%u target%u position %.9g %.9g %.9g\n",status,minimum,source_index,target_index,
            (double)placement.position[0],(double)placement.position[1],(double)placement.position[2]);
        printf("PLAYER_CHECKPOINT_REJECT source_slot %u\n",source_slot);
        }
        return status;
    }
    rf_scene_player_checkpoint_state[6]=scene_actor_body.spheres.count;rf_scene_player_checkpoint_state[7]=1;return RF_OK;
}
