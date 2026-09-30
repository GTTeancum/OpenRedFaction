/* Process-local check: two rail/rocket contacts deplete a shield; a sniper
 * shot then reaches health. Eye/input, finite ammo and initial armor are staged;
 * other actors are hidden to isolate contact from encounter choreography.
 * Grenade mode stages releases/reserve debits, then uses actual flight/contact;
 * firearm/missile modes use normal player weapon routing. */
uint32_t rf_scene_nano_test_uid,rf_scene_nano_test_mode,rf_scene_nano_fixture[25],rf_scene_nano_aim[12];
static int scene_nano_shot_stage(scene_stream *stream,uint32_t frame,
    const float original[3][3],float eye[3],float basis[3][3],uint32_t *staged)
{
    uint32_t index,sphere,axis,phase;campaign_npc_body *owner=NULL;int status;
    if(!rf_scene_nano_test_uid||*staged||(frame!=30&&frame!=60&&frame!=90))return RF_OK;
    phase=frame/30-1;
    for(index=0;index<campaign_npc_body_count;index++)if((uint32_t)campaign_seeds.records.items[index].record.uid==rf_scene_nano_test_uid){owner=campaign_npc_bodies+index;break;}
    if(!owner||!owner->registration.view||!owner->body.spheres.count)return RF_NOT_FOUND;
    for(uint32_t other=0;other<campaign_npc_body_count;other++)if(other!=index && campaign_npc_bodies[other].registration.view){
        status=campaign_set_visible(NULL,campaign_npc_bodies[other].registration.handle,0);if(status)return status;
    }
    rf_scene_nano_aim[0]=phase;rf_scene_nano_aim[1]=owner->object_flags;
    rf_scene_nano_aim[2]=owner->view.flags_810;rf_scene_nano_aim[3]=owner->body.allocated_bytes;
    memcpy(rf_scene_nano_aim+4,owner->body.state.position,12);
    rf_scene_nano_aim[7]=owner->body.spheres.count;
    if(!phase){
        status=campaign_set_invulnerable(NULL,owner->registration.handle,0);if(status)return status;
        status=campaign_set_nano_shield(NULL,owner->registration.handle,1);if(status)return status;
        owner->damage.effects.armor=rf_scene_nano_test_mode>=2?100:rf_scene_nano_test_mode==1?campaign_primary[4].damage*1.5f:150;
    }
    if(rf_scene_nano_test_mode==3 && phase==1){status=campaign_set_nano_shield(NULL,owner->registration.handle,0);if(status)return status;}
    for(sphere=0;sphere<owner->body.spheres.count;sphere++){
        const rf_physics_sphere *shape=owner->body.spheres.items+sphere;float center[3];
        for(uint32_t j=0;j<3;j++){
            center[j]=owner->body.state.position[j];
            for(uint32_t k=0;k<3;k++)center[j]+=shape->center[k]*owner->body.state.orientation[k*3+j];
        }
        for(axis=0;axis<12;axis++){
            float ray[3]={0};uint32_t blocked,direction=(axis/2)%3;
            rf_hitscan_candidate candidate={owner->registration.handle,1,&owner->registration,&owner->body,NULL};
            rf_weapon_precision_result hit;
            memcpy(eye,center,12);eye[direction]+=(axis%2?-1.f:1.f)*(shape->radius+(axis<6?2.f:.05f));
            ray[direction]=axis%2?100.f:-100.f;
            status=rf_weapon_precision_select(&campaign_registry,&candidate,1,eye,ray,1,
                RF_WEAPON_PRECISION_SNIPER,NULL,NULL,&hit);if(status)return status;
            if(!hit.count)continue;
            ++rf_scene_nano_aim[8];
            status=combat_shot_obstructed(stream,eye,ray,hit.hits[0].fraction,&blocked);
            if(status==RF_NOT_FOUND)continue;if(status)return status;
            if(blocked)continue;
            ++rf_scene_nano_aim[9];
            {
                float radius=rf_scene_nano_test_mode==3 && phase<2?campaign_remote.collision_radius:rf_scene_nano_test_mode==2?campaign_grenade.collision_radius:rf_scene_nano_test_mode==1 && phase<2?campaign_rocket.collision_radius:0;
                rf_weapon_flight_contact contact={0};uint32_t liquid=0,matched=0;
                /* The thin-ray preflight needs no alpha-texture sampler. */
                status=scene_rocket_sweep(stream,eye,ray,radius,
                    0x1004u|(radius>0 && radius<.05f?0x100u:0),&contact,&liquid,&matched);
                if(status==RF_NOT_FOUND)continue;if(status)return status;
                rf_scene_nano_aim[10]=matched;rf_scene_nano_aim[11]=contact.object;
                if(!matched||liquid||contact.object!=(SCENE_ACTOR_ROCKET_OWNER|index))continue;
            }
            memcpy(basis,original,36);memset(basis[2],0,12);basis[2][direction]=axis%2?1.f:-1.f;
            {uint32_t slot=rf_scene_nano_test_mode==3?(phase==2?9:8):rf_scene_nano_test_mode==2?5:phase==2?6:rf_scene_nano_test_mode==1?4:7,*row=rf_scene_nano_fixture+1+phase*8;
             int32_t weapon=campaign_slot_weapon(slot);
             if(weapon<0||weapon>=64)return RF_FORMAT;
             campaign_player_inventory.owned[weapon]=1;
             if(!phase||phase==2)campaign_player_inventory.loaded[weapon]=2;
             campaign_select_primary(slot);campaign_ammo_publish();
             memset(&combat_trigger,0,sizeof(combat_trigger));rf_scene_combat[6]=0;player_input.fire=1;
             memcpy(row,&owner->damage.effects.health,4);memcpy(row+1,&owner->damage.effects.armor,4);
             row[4]=(uint32_t)campaign_player_inventory.loaded[weapon];row[6]=slot;row[7]=frame;
             if(rf_scene_nano_test_mode==3){
                 uint32_t spawned;int32_t ammo=campaign_weapon_supply.definitions[weapon].ammo_type;
                 if(ammo<0||ammo>=32)return RF_FORMAT;
                 if(!phase)campaign_player_inventory.reserve[ammo]=3;
                 row[4]=(uint32_t)campaign_player_inventory.reserve[ammo];
                 if(phase<2){
                     status=scene_remote_launch(stream,frame,eye,basis[2],0,&spawned);if(status)return status;
                     if(!spawned)return RF_NOT_FOUND;player_input.fire=0;
                 }
             }
             if(rf_scene_nano_test_mode==2){
                 uint32_t flight;int32_t ammo=campaign_weapon_supply.definitions[weapon].ammo_type;
                 if(ammo<0||ammo>=32)return RF_FORMAT;
                 if(!phase)campaign_player_inventory.reserve[ammo]=3;
                 row[4]=(uint32_t)campaign_player_inventory.reserve[ammo];
                 for(flight=0;flight<SCENE_GRENADE_CAPACITY && scene_grenades[flight].flight.lifecycle.active;flight++){}
                 if(flight==SCENE_GRENADE_CAPACITY)return RF_RANGE;
                 memset(scene_grenades+flight,0,sizeof(scene_grenades[flight]));
                 status=rf_grenade_flight_launch(&scene_grenades[flight].flight,eye,basis[2],campaign_grenade.speed,
                     campaign_grenade.collision_radius,campaign_grenade.lifetime,0,phase==1?0x10u:0);if(status)return status;
                 /* Stage release directly; actual flight/contact owns all effects. */
                 --campaign_player_inventory.reserve[ammo];++rf_scene_grenades[1];
                 campaign_ammo_publish();player_input.fire=0;
             }}
            *staged=2;return RF_OK;
        }
    }
    return RF_NOT_FOUND;
}
static int scene_nano_shot_result(uint32_t frame)
{
    uint32_t phase,*row;
    if(rf_scene_nano_test_mode>=1 && frame!=59 && frame!=89 && frame!=109)return RF_OK;
    phase=frame==109?2:frame/30-1;row=rf_scene_nano_fixture+1+phase*8;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==rf_scene_nano_test_uid){
        memcpy(row+2,&campaign_npc_bodies[i].damage.effects.health,4);
        memcpy(row+3,&campaign_npc_bodies[i].damage.effects.armor,4);
        {int32_t weapon=campaign_slot_weapon(row[6]);
         row[5]=(uint32_t)(rf_scene_nano_test_mode>=2?
             campaign_player_inventory.reserve[campaign_weapon_supply.definitions[weapon].ammo_type]:
             campaign_player_inventory.loaded[weapon]);}
        ++rf_scene_nano_fixture[0];return RF_OK;
    }
    return RF_NOT_FOUND;
}
