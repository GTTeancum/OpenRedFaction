#ifndef RF_SCENE_USE_DISPATCH_INCLUDED
#define RF_SCENE_USE_DISPATCH_INCLUDED
/*4897d0/4243c0: compare actual consecutive interface tags before handlers.
 * Authored record order is the port's stable actor traversal; exact original
 * factory-list creation order remains a documented bounded difference. */
static int scene_use_score(scene_stream *stream,uint32_t handle,uint32_t kind,float radius,
    const float point[3],const float eye[3],const float forward[3],float *best)
{
    float delta[3],distance,length=0,dot=0,score;uint32_t blocked;int status;
    if(!isfinite(radius)||radius<=0)return RF_OK;
    for(uint32_t j=0;j<3;j++){
        if(!isfinite(point[j])||!isfinite(eye[j])||!isfinite(forward[j]))return RF_FORMAT;
        delta[j]=point[j]-eye[j];length+=delta[j]*delta[j];
    }
    distance=scene_use_approximate_length(delta);
    if(!isfinite(distance)||!isfinite(length)||distance>=radius)return RF_OK;
    if(kind==9)score=distance; /* Original AI response uses origin, no facing. */
    else {
        if(length<=0)return RF_OK;length=sqrtf(length);
        for(uint32_t j=0;j<3;j++)dot+=delta[j]/length*forward[j];
        if(!isfinite(dot)||dot<=0)return RF_OK;score=distance/dot;
    }
    if(!isfinite(score)||score>=*best)return RF_OK;
    if(kind==1||kind==4){
        status=combat_shot_obstructed(stream,eye,delta,1,&blocked);if(status)return status;
        if(blocked)return RF_OK;
    }
    *best=score;scene_use_edge.selected=handle;scene_use_edge.kind=kind;return RF_OK;
}
static int scene_use_static(scene_stream *stream,uint32_t handle,
    const rf_entity_class_physics *physics,const rf_static_model_tags *tags,
    const float position[3],const float basis[9],const float eye[3],const float forward[3],float *best)
{
    float pose[12];int status;int32_t tag;char name[32];
    if(!physics->use_kind)return RF_OK;
    if(physics->use_kind==9)return scene_use_score(stream,handle,9,physics->use_radius,position,eye,forward,best);
    for(uint32_t n=1;n<=tags->count;n++){
        snprintf(name,sizeof(name),"interface_%u",n);
        status=rf_static_model_tags_find(tags,(rf_model_name){name,strlen(name)},&tag);
        if(status==RF_NOT_FOUND)break;if(status)return status;
        status=rf_static_model_tag_place(tags,tag,basis,position,pose);if(status)return status;
        status=scene_use_score(stream,handle,physics->use_kind,physics->use_radius,pose+9,eye,forward,best);if(status)return status;
    }
    return RF_OK;
}
static int scene_use_prepare(scene_stream *stream,uint32_t frame,const rf_motion_controller *controller)
{
    float eye[3],basis[3][3],best=FLT_MAX;int status;
    if(scene_use_edge.prepared)return RF_OK;
    scene_use_pose_frame=UINT32_MAX;scene_use_edge.prepared=1;scene_use_edge.selected=UINT32_MAX;scene_use_edge.kind=0;
    if(!campaign_spawn){scene_use_edge.prepared=0;return RF_OK;}
    /* Frame0 exists to build and restore scene ownership. Never originate a
     * heal (or replay a saved held press) before the final load commit. */
    if(!frame){scene_use_edge.player=campaign_player_object.handle;
        scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;}
    if(!scene_use_edge.filtered||
       campaign_endgame.phase||campaign_cutscene_runtime.active||campaign_defuse.active||
       campaign_player_damage.state.effects.health<=0){scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;}
    if(campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view){
        scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;
    }
    /* Raw owner changes never manufacture an edge for a new generation. */
    if(scene_use_edge.player!=UINT32_MAX&&scene_use_edge.player!=campaign_player_object.handle){
        scene_use_edge.player=campaign_player_object.handle;scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;
    }
    scene_use_edge.player=campaign_player_object.handle;
    if(scene_turret_player_active()){
        scene_use_edge.selected=scene_turret_player.host;scene_use_edge.kind=4;
        scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;
    }
    if(scene_driller_active(stream)){
        scene_use_edge.selected=stream->driller_runtime->entry.host.handle;scene_use_edge.kind=1;
        scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;
    }
    if(campaign_player_view.linked_handle!=-1){scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;return RF_OK;}
    if(!scene_use_edge.edge)return RF_OK;
    /* This queries the current body/eye once. actor_follow_view reuses it for
     * this frame's on-foot camera, avoiding a second look integration. */
    status=actor_listener_pose(stream,frame,controller,eye,basis);if(status)return status;
    memcpy(scene_use_eye,eye,12);memcpy(scene_use_basis,basis,36);scene_use_pose_frame=frame;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++){
        const rf_entity_class_physics *physics;
        const uint32_t cls=campaign_seeds.items[i].class_index;
        const uint32_t uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
        if(cls>=campaign_seeds.class_count)return RF_FORMAT;
        physics=&campaign_seeds.classes[cls].physics;if(!physics->use_kind)continue;
        if(i<campaign_npc_body_count&&scene_medic_live(campaign_npc_bodies+i)){
            campaign_npc_body *o=campaign_npc_bodies+i;float tag[12];int32_t id;char name[32];
            rf_corpse_surface_source source={0};
            if(o->registration.handle==campaign_player_object.handle||o->damage.effects.health<=0||
               (o->view.flags_810&1u)||(o->object_flags&(2u|0x4000u)))continue;
            if(physics->use_kind==9){status=scene_use_score(stream,o->registration.handle,9,
                physics->use_radius,o->body.state.position,eye,basis[2],&best);if(status)return status;continue;}
            source.model=i+1;memcpy(source.position,o->body.state.position,12);
            memcpy(source.basis,o->body.state.orientation,36);
            /* Actual medic01 has interface_1 only. No invented body fallback. */
            for(uint32_t n=1;n<=64;n++){
                snprintf(name,sizeof(name),"interface_%u",n);status=rf_scene_corpse_file_tag(i+1,name,0,&id);
                if(status==RF_NOT_FOUND)break;if(status)return status;
                status=campaign_file_tag_pose(&source,id,tag);if(status)return status;
                status=scene_use_score(stream,o->registration.handle,physics->use_kind,physics->use_radius,
                    tag+9,eye,basis[2],&best);if(status)return status;
            }
            continue;
        }
        if(stream->driller_runtime&&uid==(uint32_t)campaign_authored_vehicle_uid){
            scene_driller_runtime *r=stream->driller_runtime;
            if(!r->entry.host.alive||r->damage.damage.state.effects.health<=0||
               (r->damage.damage.object_flags&(2u|0x4000u)))continue;
            status=scene_use_static(stream,r->entry.host.handle,physics,&stream->driller->tags,
                r->physics.state.position,r->physics.state.orientation,eye,basis[2],&best);if(status)return status;
            continue;
        }
        for(uint32_t j=0;j<scene_turret_authored_count;j++){
            scene_turret_owner *o=scene_turrets+j;
            if(o->uid!=uid)continue;
            if(scene_turret_lookup(o->registration.handle)!=o||o->dead||o->damage.effects.health<=0||
               (o->view.flags_7c&(2u|0x4000u)))break;
            if(o->live_model>=scene_turret_model_count)return RF_FORMAT;
            status=scene_use_static(stream,o->registration.handle,physics,&scene_turret_models[o->live_model].tags,
                o->position,o->basis,eye,basis[2],&best);if(status)return status;break;
        }
        for(uint32_t j=0;j<campaign_passive_vehicle_count;j++){
            scene_passive_vehicle *o=campaign_passive_vehicles+j;const scene_driller_resources *resource;
            if(o->uid!=uid)continue;
            if(o->resource_kind>=6)return RF_FORMAT;
            if(rf_object_registry_lookup(&campaign_registry,o->handle)!=o||o->damage.destroyed||
               o->damage.state.effects.health<=0||(o->damage.object_flags&(2u|0x4000u)))break;
            resource=campaign_selected_vehicle_resource_kind()==o->resource_kind?stream->driller:
                stream->passive_vehicle_resources[o->resource_kind];
            if(!resource)break;
            status=scene_use_static(stream,o->handle,physics,&resource->tags,o->pose.position,
                o->pose.input_matrix,eye,basis[2],&best);if(status)return status;break;
        }
    }
    if(scene_use_edge.selected!=UINT32_MAX){
        /*4a1660 consumes SP medic6/AI-response9 even on handler refusal.
         * Vehicle1/turret4 admission failure and switch2/command3 instead
         * retain4a1970 trigger fallback; selection alone is not consumption. */
        if(scene_use_edge.kind==6||scene_use_edge.kind==9)scene_use_claim();
        if(scene_use_edge.kind==6)for(uint32_t i=0;i<campaign_npc_body_count;i++){
            campaign_npc_body *o=campaign_npc_bodies+i;
            if(o->registration.handle==scene_use_edge.selected&&scene_medic_live(o))return scene_medic_accept(o,frame);
        }
    }
    return RF_OK;
}
#endif
