/* Process-local check: two rail contacts deplete a150-armor shield; a sniper
 * shot then reaches health. Only eye/input, finite ammo and initial armor are
 * staged. The normal player weapon, collision and damage paths do the work. */
uint32_t rf_scene_nano_test_uid,rf_scene_nano_fixture[25];
static int scene_nano_shot_stage(scene_stream *stream,uint32_t frame,
    const float original[3][3],float eye[3],float basis[3][3],uint32_t *staged)
{
    uint32_t index,sphere,axis,phase;campaign_npc_body *owner=NULL;int status;
    if(!rf_scene_nano_test_uid||*staged||(frame!=30&&frame!=60&&frame!=90))return RF_OK;
    phase=frame/30-1;
    for(index=0;index<campaign_npc_body_count;index++)if((uint32_t)campaign_seeds.records.items[index].record.uid==rf_scene_nano_test_uid){owner=campaign_npc_bodies+index;break;}
    if(!owner||!owner->registration.view||!owner->body.spheres.count)return RF_NOT_FOUND;
    if(!phase){
        status=campaign_set_invulnerable(NULL,owner->registration.handle,0);if(status)return status;
        status=campaign_set_nano_shield(NULL,owner->registration.handle,1);if(status)return status;
        owner->damage.effects.armor=150;
    }
    for(sphere=0;sphere<owner->body.spheres.count;sphere++){
        const rf_physics_sphere *shape=owner->body.spheres.items+sphere;float center[3];
        for(uint32_t j=0;j<3;j++){
            center[j]=owner->body.state.position[j];
            for(uint32_t k=0;k<3;k++)center[j]+=shape->center[k]*owner->body.state.orientation[k*3+j];
        }
        for(axis=0;axis<6;axis++){
            float ray[3]={0};uint32_t blocked,direction=axis/2;
            rf_hitscan_candidate candidate={owner->registration.handle,1,&owner->registration,&owner->body,NULL};
            rf_weapon_precision_result hit;
            memcpy(eye,center,12);eye[direction]+=(axis%2?-1.f:1.f)*(shape->radius+2.f);
            ray[direction]=axis%2?100.f:-100.f;
            status=rf_weapon_precision_select(&campaign_registry,&candidate,1,eye,ray,1,
                RF_WEAPON_PRECISION_SNIPER,NULL,NULL,&hit);if(status)return status;
            if(!hit.count)continue;
            status=combat_shot_obstructed(stream,eye,ray,hit.hits[0].fraction,&blocked);if(status)return status;
            if(blocked)continue;
            memcpy(basis,original,36);memset(basis[2],0,12);basis[2][direction]=axis%2?1.f:-1.f;
            {uint32_t slot=phase==2?6:7,*row=rf_scene_nano_fixture+1+phase*8;
             int32_t weapon=campaign_slot_weapon(slot);
             if(weapon<0||weapon>=64)return RF_FORMAT;
             campaign_player_inventory.owned[weapon]=1;
             if(!phase||phase==2)campaign_player_inventory.loaded[weapon]=2;
             campaign_select_primary(slot);campaign_ammo_publish();
             memset(&combat_trigger,0,sizeof(combat_trigger));rf_scene_combat[6]=0;player_input.fire=1;
             memcpy(row,&owner->damage.effects.health,4);memcpy(row+1,&owner->damage.effects.armor,4);
             row[4]=(uint32_t)campaign_player_inventory.loaded[weapon];row[6]=slot;row[7]=frame;}
            *staged=2;return RF_OK;
        }
    }
    return RF_NOT_FOUND;
}
static int scene_nano_shot_result(uint32_t frame)
{
    uint32_t *row=rf_scene_nano_fixture+1+(frame/30-1)*8;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==rf_scene_nano_test_uid){
        memcpy(row+2,&campaign_npc_bodies[i].damage.effects.health,4);
        memcpy(row+3,&campaign_npc_bodies[i].damage.effects.armor,4);
        row[5]=(uint32_t)campaign_player_inventory.loaded[campaign_slot_weapon(row[6])];
        ++rf_scene_nano_fixture[0];return RF_OK;
    }
    return RF_NOT_FOUND;
}
