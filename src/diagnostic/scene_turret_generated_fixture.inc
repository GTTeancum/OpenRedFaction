#ifndef RF_SCENE_TURRET_GENERATED_FIXTURE_INCLUDED
#define RF_SCENE_TURRET_GENERATED_FIXTURE_INCLUDED
/* Diagnostic opt-in only. Resolve the actual dependent owner by stable base
 * key, then stage handgun-valued hitscan rays through shared model collision,
 * cover and damage. No authored head, health write, forced immunity or AI edit.
 * This checks direct-hit/death services, not player aim/input/ammo/cadence. */
uint32_t rf_scene_turret_generated_test_uid;
/* baseUID,initialized,basehandle,headhandle,rays,hits,covered,initialhead,
 * initialbase,finalhead,finalbase,headdead,shotsatdeath,deathframe,errors,
 * finalframe,damagekind,weaponamount,appliedsum,lastcontacthandle */
uint32_t rf_scene_turret_generated_test[20];
static int scene_turret_generated_fixture(scene_stream *stream,uint32_t frame)
{
    scene_turret_generated_key key={rf_scene_turret_generated_test_uid,SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD};
    scene_turret_generated_binding *b;scene_turret_owner *head;campaign_npc_body *base;
    uint32_t handle,axis,side,i,matched,blocked,handled;float center[3],offset[3],start[3],delta[3],applied,sum;
    rf_weapon_flight_contact contact;int status;
    if(!rf_scene_turret_generated_test_uid)return RF_OK;
    if(!frame)memset(rf_scene_turret_generated_test,0,sizeof(rf_scene_turret_generated_test));
    status=scene_turret_generated_resolve(key,&handle);if(status)goto fail;
    b=scene_turret_generated_find(handle);head=scene_turret_lookup(handle);
    base=b?scene_turret_generated_base(b):NULL;if(!head||!base){status=RF_NOT_FOUND;goto fail;}
    if(!rf_scene_turret_generated_test[1]) {
        if(head->dead||head->damage.effects.health<=0||base->damage.effects.health<=0||campaign_primary[0].damage<=0||
           campaign_primary[0].damage_kind<0||campaign_primary[0].damage_kind>10){status=RF_FORMAT;goto fail;}
        rf_scene_turret_generated_test[0]=key.base_uid;rf_scene_turret_generated_test[1]=1;
        rf_scene_turret_generated_test[2]=b->base_handle;rf_scene_turret_generated_test[3]=handle;
        memcpy(rf_scene_turret_generated_test+7,&head->damage.effects.health,4);
        memcpy(rf_scene_turret_generated_test+8,&base->damage.effects.health,4);
        rf_scene_turret_generated_test[16]=(uint32_t)campaign_primary[0].damage_kind;
        memcpy(rf_scene_turret_generated_test+17,&campaign_primary[0].damage,4);
    }
    if(frame>=60&&!head->dead) {
        if(head->live_model>=scene_turret_model_count){status=RF_FORMAT;goto fail;}
        scene_turret_world_vector(head->basis,scene_turret_models[head->live_model].render.bound,offset);
        for(i=0;i<3;i++)center[i]=head->position[i]+offset[i];
        /* Six bounded directions tolerate the animated barrel and real level
         * cover. Only the first actual unobstructed head triangle receives a
         * shot; failure to find one is evidence, never bypassed by direct damage. */
        for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
            memset(&contact,0,sizeof(contact));matched=blocked=handled=0;applied=0;
            memcpy(start,center,12);memset(delta,0,sizeof(delta));
            start[axis]+=side?3.f:-3.f;delta[axis]=side?-6.f:6.f;
            ++rf_scene_turret_generated_test[4];
            status=scene_driller_firearm_select(campaign_player_object.handle,start,delta,1,&contact,&matched);if(status)goto fail;
            if(!matched||contact.object!=SCENE_TURRET_PROJECTILE_OWNER||contact.face!=handle)continue;
            status=combat_shot_obstructed(stream,start,delta,contact.hit.fraction,&blocked);if(status)goto fail;
            if(blocked){++rf_scene_turret_generated_test[6];continue;}
            status=scene_driller_projectile_damage(&contact,campaign_player_object.handle,campaign_primary[0].damage,
                campaign_primary[0].damage_kind,frame,&handled,&applied);if(status)goto fail;
            if(!handled||applied<=0){status=RF_FORMAT;goto fail;}
            ++rf_scene_turret_generated_test[5];rf_scene_turret_generated_test[19]=contact.face;
            memcpy(&sum,rf_scene_turret_generated_test+18,4);sum+=applied;
            memcpy(rf_scene_turret_generated_test+18,&sum,4);goto record;
        }
        status=RF_NOT_FOUND;goto fail;
    }
record:
    memcpy(rf_scene_turret_generated_test+9,&head->damage.effects.health,4);
    memcpy(rf_scene_turret_generated_test+10,&base->damage.effects.health,4);
    rf_scene_turret_generated_test[11]=head->dead;
    if(head->dead&&!rf_scene_turret_generated_test[13]) {
        rf_scene_turret_generated_test[12]=rf_scene_turret_shots[0];
        rf_scene_turret_generated_test[13]=frame;
    }
    rf_scene_turret_generated_test[15]=frame;return RF_OK;
fail:++rf_scene_turret_generated_test[14];return status;
}
#endif
