#ifndef SCENE_ACTIVE_VEHICLE_VISIBILITY_FIXTURE_INCLUDED
#define SCENE_ACTIVE_VEHICLE_VISIBILITY_FIXTURE_INCLUDED
/* Read-only observations for the enemy-free APC9627 visibility fixture.
 * Include after the active runtime/query definitions. Call after the late
 * frame-zero load. The explicit campaign-active-visibility.bin opt-in is
 * four little-endian words: AVIS, phase(1source/2load), case(0visible/1hidden),0.
 * Real authored events and ordinary replay Use own every state transition.
 * Queries use the actual admitted collider, never a manually reconstructed
 * substitute. Query diagnostics may advance; gameplay state is untouched.
 * A300-frame presentation's final physics sample is298. */
/* config reads,phase,case,samples,UID,host,enabled,status */
uint32_t rf_scene_active_vehicle_visibility_fixture[8];
uint32_t rf_scene_active_vehicle_visibility_samples[9][56];
/* runs,frame,player,host,prior seated probes,negative status/clear,positive
 * status/clear,unchanged mask,actual head index/radius,then the exact20-word
 * first seated-path probe for the downward floor-blocked query. */
uint32_t rf_scene_active_vehicle_seat_clearance[32];
static void scene_active_vehicle_seat_clearance_tick(scene_stream *s,uint32_t frame)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;scene_driller_entry_exit before;
    rf_physics_body_state player_before;rf_vehicle_rigid_state host_before;
    scene_vehicle_pose from,down,seat;uint32_t *a=rf_scene_active_vehicle_seat_clearance;
    uint32_t i,index=0,clear=0,damage_flags,view_flags;int status;
    if(frame!=150||a[0]||!rf_scene_active_vehicle_visibility_fixture[6]||
       rf_scene_active_vehicle_visibility_fixture[1]!=1||!r)return;
    a[0]=1;a[1]=frame;a[2]=campaign_player_object.handle;a[3]=r->entry.host.handle;
    a[4]=rf_scene_seated_path_probe[0];
    if(a[4]||r->entry.session.active||!scene_actor_body.spheres.items||
       !scene_actor_body.spheres.count||scene_actor_body.spheres.count>64){a[5]=(uint32_t)RF_FORMAT;return;}
    before=r->entry;player_before=scene_actor_body.state;host_before=r->physics.state;
    damage_flags=r->damage.damage.object_flags;view_flags=r->entry.view.flags_7c;
    for(i=1;i<scene_actor_body.spheres.count;i++)
        if(scene_actor_body.spheres.items[i].center[1]>scene_actor_body.spheres.items[index].center[1])index=i;
    a[10]=index;memcpy(a+11,&scene_actor_body.spheres.items[index].radius,4);
    memcpy(from.position,scene_actor_body.state.position,12);memcpy(from.basis,scene_actor_body.state.orientation,36);
    down=from;down.position[1]-=8;
    /* Use the real entry composition, including normal world/actor admission.
     * Only this stack endpoint moves. The actual head sphere crosses the known
     * central floor; own-host exclusion cannot remove that terrain contact. */
    status=scene_vehicle_seated_path(&r->entry,&from,&down,&clear);
    a[5]=(uint32_t)status;a[6]=clear;
    memcpy(a+12,rf_scene_seated_path_probe,sizeof(rf_scene_seated_path_probe));
    clear=0;status=scene_driller_entry_seat(&r->entry,r->entry.host.handle,&seat);
    if(!status)status=scene_vehicle_seated_path(&r->entry,&from,&seat,&clear);
    a[7]=(uint32_t)status;a[8]=clear;
    if(!memcmp(&player_before,&scene_actor_body.state,sizeof(player_before)))a[9]|=1u;
    if(!memcmp(&host_before,&r->physics.state,sizeof(host_before)))a[9]|=2u;
    if(damage_flags==r->damage.damage.object_flags&&view_flags==r->entry.view.flags_7c)a[9]|=4u;
    if(!memcmp(&before,&r->entry,sizeof(before)))a[9]|=8u;
}
static uint32_t scene_active_vehicle_visibility_fixture_empty_npcs(void)
{
    for(uint32_t i=0;i<campaign_npc_body_count;i++)
        if(campaign_npc_bodies[i].registration.view)return 0;
    return 1; /* Static vehicle seeds still occupy unused pose/body slots. */
}
static void scene_active_vehicle_visibility_fixture_open(void)
{
    uint32_t config[4]={0};FILE *file;int trailing,status=RF_OK;
    memset(rf_scene_active_vehicle_visibility_fixture,0,sizeof(rf_scene_active_vehicle_visibility_fixture));
    memset(rf_scene_active_vehicle_visibility_samples,0,sizeof(rf_scene_active_vehicle_visibility_samples));
    memset(rf_scene_active_vehicle_seat_clearance,0,sizeof(rf_scene_active_vehicle_seat_clearance));
#ifdef RF_IMAGE_XBOX_NATIVE
    errno=0;file=fopen("D:\\campaign-active-visibility.bin","rb");
    if(!file){if(errno!=ENOENT)rf_scene_active_vehicle_visibility_fixture[7]=(uint32_t)RF_IO;return;}
#else
    return;
#endif
    rf_scene_active_vehicle_visibility_fixture[0]=1;
    if(fread(config,1,sizeof(config),file)!=sizeof(config))status=RF_FORMAT;
    trailing=fgetc(file);if(trailing!=EOF||ferror(file))status=RF_FORMAT;
    if(fclose(file))status=RF_IO;
    if(config[0]!=0x53495641u||(config[1]!=1&&config[1]!=2)||config[2]>1||config[3])status=RF_FORMAT;
    rf_scene_active_vehicle_visibility_fixture[1]=config[1];
    rf_scene_active_vehicle_visibility_fixture[2]=config[2];
    rf_scene_active_vehicle_visibility_fixture[6]=!status;
    rf_scene_active_vehicle_visibility_fixture[7]=(uint32_t)status;
}
static void scene_active_vehicle_visibility_fixture_tick(scene_stream *s,uint32_t frame)
{
    static const uint32_t frames[9]={0,20,40,100,118,150,200,250,298};
    scene_driller_runtime *r;scene_driller_entry_exit *e;
    rf_weapon_flight_contact projectile={0};rf_geometry_body_hit actor={0};
    rf_collision_body_sphere sphere={{0,0,0},.1f};rf_collision_body_query query={0};
    float center[3],start[3],delta[3]={0},target[3]={0},health=0,distance;
    uint32_t i,j,*row,matched=0;int status;
    if(!frame)scene_active_vehicle_visibility_fixture_open();
    if(!rf_scene_active_vehicle_visibility_fixture[6])return;
    if(!campaign_spawn||strcmp(campaign_current_level,"L1S3.rfl")||campaign_authored_vehicle_uid!=9627||
       campaign_passive_vehicle_count||!scene_active_vehicle_visibility_fixture_empty_npcs()||!s||!(r=s->driller_runtime))return;
    for(i=0;i<9&&frames[i]!=frame;i++){ /* Bounded observation slots. */ }
    if(i==9)return;e=&r->entry;row=rf_scene_active_vehicle_visibility_samples[i];
    ++rf_scene_active_vehicle_visibility_fixture[3];
    rf_scene_active_vehicle_visibility_fixture[4]=9627;
    rf_scene_active_vehicle_visibility_fixture[5]=e->host.handle;
    row[0]=1;row[1]=frame;row[2]=(uint32_t)campaign_authored_vehicle_uid;row[3]=e->host.handle;
    row[4]=rf_object_registry_lookup(&campaign_registry,e->host.handle)==&e->registration;
    row[5]=rf_entity_lookup(&campaign_entities,(int32_t)e->host.handle)==&e->view;
    memcpy(row+6,&r->damage.damage.state.effects.health,4);memcpy(row+7,&r->damage.damage.state.effects.armor,4);
    row[8]=r->damage.damage.object_flags;row[9]=e->view.flags_7c;
    row[10]=e->session.active;row[11]=e->session.host;row[12]=e->session.driver;row[13]=e->host.driver;
    row[14]=campaign_player_object.handle;row[15]=(uint32_t)campaign_player_view.linked_handle;
    row[16]=e->player.host;row[17]=e->player.control;
    row[18]=rf_scene_vehicle_entry_probe[0];row[19]=rf_scene_vehicle_entry_probe[4];
    row[20]=rf_scene_vehicle_state[1];row[21]=rf_scene_vehicle_state[2];
    memcpy(row+22,rf_scene_active_vehicle_draw,16);
    row[31]=scene_campaign_vehicle_target(s,e->host.handle,target,&health);memcpy(row+32,&health,4);
    row[33]=r->physics.sphere_count;
    memcpy(row+42,r->physics.state.position,12);memcpy(row+45,scene_actor_body.state.position,12);
    row[48]=rf_scene_vehicle_entry_probe[1];row[49]=rf_scene_vehicle_entry_probe[14];
    row[50]=rf_scene_setup_result[0];row[51]=rf_scene_active_vehicle_visibility[0];
    row[52]=e->view.flags_814;row[53]=r->damage.damage.destroyed;row[54]=e->host.alive;
    row[55]=rf_scene_vehicle_entry_probe[5];
    if(!r->physics.sphere_count||r->physics.sphere_count>8){row[26]=(uint32_t)RF_FORMAT;return;}
    scene_driller_point(r->physics.state.position,r->physics.state.orientation,r->physics.spheres[0].center,center);
    memcpy(row+38,center,12);memcpy(row+41,&r->physics.spheres[0].radius,4);
    distance=r->physics.spheres[0].radius*2+1;
    memcpy(start,center,12);start[0]-=distance;delta[0]=distance*2;
    status=scene_driller_projectile_query(UINT32_MAX,start,delta,0,1,&projectile,&matched);
    row[26]=(uint32_t)status;row[27]=matched;row[28]=projectile.object;row[29]=projectile.face;
    memcpy(row+30,&projectile.hit.fraction,4);
    query.spheres=&sphere;query.count=1;query.flags=4;query.limit=1;
    for(j=0;j<3;j++){query.start[j]=start[j];query.end[j]=start[j]+delta[j];query.matrix[j][j]=1;}
    matched=0;status=scene_driller_player_collision(s,&query,&actor,&matched);
    row[34]=(uint32_t)status;row[35]=matched;row[36]=actor.contact.object_id;
    memcpy(row+37,&actor.contact.fraction,4);
    scene_active_vehicle_seat_clearance_tick(s,frame);
}
#endif
