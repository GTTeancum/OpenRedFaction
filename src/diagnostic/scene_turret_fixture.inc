#ifndef SCENE_TURRET_FIXTURE_INC
#define SCENE_TURRET_FIXTURE_INC
/* Opt-in process-local collision/damage fixture. The authored owner keeps its
 * class, health, armor, damage factors and models. Only its AI action is staged
 * idle. Rays use ordinary handgun damage and shared nearest-contact/cover/
 * damage dispatch; this is not a weapon input, ammo or fire-rate test. */
uint32_t rf_scene_turret_test_uid;
/* uid,initialized,rays,hits,cover,handled,dead,initial health,final health,
 * initial armor,final armor,live model,dead model,selected model,first health,
 * applied sum,factor,weapon damage,kind,frame,errors,post-death miss */
uint32_t rf_scene_turret_test[22];
static int scene_turret_fixture(scene_stream *stream,uint32_t frame)
{
    scene_turret_owner *o;uint32_t handle,matched=0,blocked=0,handled=0,i;int status;
    rf_weapon_flight_contact contact={0};float eye[3],start[3],delta[3]={0,0,-6},applied=0,sum;
    uint32_t affiliation,alive;
    if(!rf_scene_turret_test_uid)return RF_OK;
    if(!frame)memset(rf_scene_turret_test,0,sizeof(rf_scene_turret_test));
    handle=scene_turret_uid_handle(rf_scene_turret_test_uid);o=scene_turret_lookup(handle);
    if(!o){++rf_scene_turret_test[20];return RF_NOT_FOUND;}
    o->view.action_520=1; /* Catatonic: bounded damage target, no autonomous fire. */
    if(!rf_scene_turret_test[1]) {
        if(o->dead || o->damage.effects.health<=0 || o->dead_model>=scene_turret_model_count ||
           o->dead_model==o->live_model || campaign_primary[0].damage<=0 ||
           campaign_primary[0].damage_kind<0 || campaign_primary[0].damage_kind>10)return RF_FORMAT;
        rf_scene_turret_test[0]=o->uid;rf_scene_turret_test[1]=1;
        memcpy(rf_scene_turret_test+7,&o->damage.effects.health,4);
        memcpy(rf_scene_turret_test+9,&o->damage.effects.armor,4);
        rf_scene_turret_test[11]=o->live_model;rf_scene_turret_test[12]=o->dead_model;
        memcpy(rf_scene_turret_test+16,o->damage_factors+campaign_primary[0].damage_kind,4);
        memcpy(rf_scene_turret_test+17,&campaign_primary[0].damage,4);
        rf_scene_turret_test[18]=(uint32_t)campaign_primary[0].damage_kind;
    }
    status=scene_turret_target(handle,eye,&affiliation,&alive);if(status)goto fail;
    for(i=0;i<3;i++)start[i]=eye[i]+(i==2?3:0);
    if(frame>=30 && !o->dead) {
        ++rf_scene_turret_test[2];
        status=scene_driller_firearm_select(campaign_player_object.handle,start,delta,1,&contact,&matched);if(status)goto fail;
        if(!matched){status=RF_NOT_FOUND;goto fail;}
        if(contact.object!=SCENE_TURRET_PROJECTILE_OWNER || contact.face!=handle){status=RF_FORMAT;goto fail;}
        ++rf_scene_turret_test[3];
        status=combat_shot_obstructed(stream,start,delta,contact.hit.fraction,&blocked);if(status)goto fail;
        if(blocked){++rf_scene_turret_test[4];status=RF_FORMAT;goto fail;}
        status=scene_driller_projectile_damage(&contact,campaign_player_object.handle,campaign_primary[0].damage,
            campaign_primary[0].damage_kind,frame,&handled,&applied);if(status)goto fail;
        if(!handled || applied<=0){status=RF_FORMAT;goto fail;}
        ++rf_scene_turret_test[5];memcpy(&sum,rf_scene_turret_test+15,4);sum+=applied;
        memcpy(rf_scene_turret_test+15,&sum,4);
        if(rf_scene_turret_test[5]==1)memcpy(rf_scene_turret_test+14,&o->damage.effects.health,4);
    }else if(o->dead && !rf_scene_turret_test[21]) {
        status=scene_driller_firearm_select(campaign_player_object.handle,start,delta,1,&contact,&matched);if(status)goto fail;
        if(matched){status=RF_FORMAT;goto fail;}
        rf_scene_turret_test[21]=1;
    }
    rf_scene_turret_test[6]=o->dead;rf_scene_turret_test[13]=o->model;
    memcpy(rf_scene_turret_test+8,&o->damage.effects.health,4);
    memcpy(rf_scene_turret_test+10,&o->damage.effects.armor,4);
    rf_scene_turret_test[19]=frame;return RF_OK;
fail:
    ++rf_scene_turret_test[20];return status;
}
#endif
