/* Private scene RFCP eligibility/catalog checks; no assets/game/emulator. */
#include "../src/diagnostic/scene.c"
#include "rf/checkpoint_file.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scene RFCP line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned char transport_buffer[RF_CHECKPOINT_FILE_MAX];
static int transport_accept(const void *p,uint32_t n,void *context)
{(void)p;(void)n;(void)context;return RF_OK;}
static int transport_physical_reject(const void *p,uint32_t n,void *context)
{(void)p;(void)n;(void)context;return scene_checkpoint_player_semantic_status(RF_NOT_FOUND);}
static int transport_semantic_test(void)
{
 const char *base="rf-player-checkpoint-semantic-test";const char *paths[2]={"rf-player-checkpoint-semantic-test.0","rf-player-checkpoint-semantic-test.1"};
 rf_checkpoint_file_selection token;uint32_t bytes,i;FILE *file;
 for(i=0;i<2;i++){file=fopen(paths[i],"rb");if(file){fclose(file);fprintf(stderr,"Refusing existing semantic test slots\n");return 1;}}
 CHECK(scene_checkpoint_player_semantic_status(RF_IO)==RF_IO&&scene_checkpoint_player_semantic_status(RF_RANGE)==RF_RANGE);
 CHECK(rf_checkpoint_file_load(base,transport_buffer,sizeof(transport_buffer),&bytes,transport_physical_reject,NULL,&token)==RF_NOT_FOUND);
 CHECK(token.ready&&bytes==0);
 CHECK(!rf_checkpoint_file_store(base,"present",7,transport_accept,NULL,&token));
 CHECK(!rf_checkpoint_file_store(base,"present",7,transport_accept,NULL,&token));
 CHECK(rf_checkpoint_file_load(base,transport_buffer,sizeof(transport_buffer),&bytes,transport_physical_reject,NULL,&token)==RF_FORMAT);
 CHECK(!token.ready);CHECK(!remove(paths[0])&&!remove(paths[1]));return 0;
}
static int render_exclusion_test(void)
{
 scene_stream s={0};rf_geometry geometry={0};rf_scene_world_geometry world={0};
 const rf_scene_world_geometry *old=actor_follow_world;
 uint32_t offsets[8]={11,29,37,51,75,93,119,131},copy[8],remove_ids[3]={1,4,6};
 uint32_t duplicate[2]={1,1},invalid[1]={8};
 memcpy(copy,offsets,sizeof(copy));geometry.faces=8;geometry.face_offsets=offsets;
 world.world=&geometry;actor_follow_world=&world;s.geometry=&geometry;
 CHECK(scene_terrain_render_exclude(&s,duplicate,2)==RF_FORMAT && !s.terrain_face_offsets);
 CHECK(scene_terrain_render_exclude(&s,invalid,1)==RF_RANGE && !s.terrain_face_offsets);
 CHECK(!scene_terrain_render_exclude(&s,remove_ids,3));
 CHECK(s.terrain_geometry.faces==5 && s.terrain_render.world==&s.terrain_geometry);
 CHECK(s.terrain_geometry.face_offsets[0]==11 && s.terrain_geometry.face_offsets[1]==37 &&
       s.terrain_geometry.face_offsets[2]==51 && s.terrain_geometry.face_offsets[3]==93 && s.terrain_geometry.face_offsets[4]==131);
 CHECK(!memcmp(offsets,copy,sizeof(copy)) && geometry.faces==8);
 CHECK(scene_terrain_render_exclude(&s,remove_ids,3)==RF_RANGE);
 free(s.terrain_face_offsets);actor_follow_world=old;return 0;
}
int main(void)
{
 static const char *const names[5]={"12mm handgun","Assault Rifle","Riot Stick","Shotgun","Rocket Launcher"};
 CHECK(!render_exclusion_test());
 scene_stream s={0};rf_player_checkpoint_catalog c;rf_physics_sphere sphere={{0,0,0},.6f,0,0};uint32_t i;
 rf_scene_dev_room_enabled=campaign_spawn=1;rf_scene_water_test_enabled=0;strcpy(campaign_current_level,"glass_house.rfl");
 s.terrain=(rf_geomod_terrain *)(uintptr_t)1; /* Scope predicate checks existence only. */
 scene_actor_body.allocated_bytes=sizeof(scene_actor_body);scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
 campaign_player_damage.state.effects.health=50;campaign_player_damage.state.effects.class_health=100;campaign_player_damage.state.effects.class_armor=100;
 campaign_player_view.linked_handle=-1;rf_scene_actor_landing[1]=1;
 CHECK(!scene_checkpoint_player_scope(&s,1));campaign_crouched=1;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);campaign_crouched=0;
 scene_actor_body.state.velocity[0]=.01f;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);scene_actor_body.state.velocity[0]=0;
 combat_trigger.cooldown=1;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);combat_trigger.cooldown=0;
 rf_scene_rockets[3]=1;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);rf_scene_rockets[3]=0;
 campaign_player_view.linked_handle=123;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);campaign_player_view.linked_handle=-1;
 rf_scene_actor_landing[1]=3;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);CHECK(!scene_checkpoint_player_scope(&s,0));rf_scene_actor_landing[1]=1;
 campaign_movers.count=1;CHECK(scene_checkpoint_player_scope(&s,0)==RF_RANGE);campaign_movers.count=0;
 rf_scene_clutter_bodies[1]=1;CHECK(scene_checkpoint_player_scope(&s,0)==RF_RANGE);rf_scene_clutter_bodies[1]=0;
 rf_scene_water_test_enabled=1;CHECK(scene_checkpoint_player_scope(&s,0)==RF_RANGE);rf_scene_water_test_enabled=0;
 actor_look.state.angular_velocity[1]=.1f;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);actor_look.state.angular_velocity[1]=0;
 actor_look.state.pending_pitch=.1f;CHECK(scene_checkpoint_player_scope(&s,1)==RF_RANGE);actor_look.state.pending_pitch=0;
 {
  rf_player_checkpoint player={0};rf_checkpoint_placement placement;float basis[9]={1,0,0,0,1,0,0,0,1};
  scene_actor_body.state.state_124=0x1000u|0x40u;
  scene_checkpoint_placement(&s,&player,basis,&placement);
  CHECK(placement.query_flags==(0x40u|4u));CHECK(scene_actor_body.state.state_124==(0x1000u|0x40u));
  CHECK(placement.spheres==&sphere&&placement.count==1);
  scene_actor_body.state.state_124=0x80u;scene_checkpoint_placement(&s,&player,basis,&placement);
  CHECK(placement.query_flags==(0x80u|4u)); /* Do not silently discard unsupported alpha policy. */
  scene_actor_body.state.state_124=0;
 }
 rf_scene_player_checkpoint_enabled=1;rf_scene_player_checkpoint_state[1]=1;
 rf_scene_actor_landing[1]=3;scene_actor_body.state.flags=0x20600001u;
 campaign_support_handle=99;campaign_support_velocity[1]=1;scene_actor_body.state.velocity[1]=.001f;
 scene_checkpoint_player_locomotion();
 CHECK(rf_scene_actor_landing[1]==1&&rf_scene_actor_landing[2]==0);
 CHECK(scene_actor_body.state.flags==0x20000001u&&!campaign_support_handle&&campaign_support_velocity[1]==0&&scene_actor_body.state.velocity[1]==.001f);
 rf_scene_player_checkpoint_enabled=0;rf_scene_actor_landing[1]=3;scene_checkpoint_player_locomotion();CHECK(rf_scene_actor_landing[1]==3);
 rf_scene_player_checkpoint_enabled=1;rf_scene_player_checkpoint_state[1]=0;scene_checkpoint_player_locomotion();CHECK(rf_scene_actor_landing[1]==3);
 rf_scene_player_checkpoint_enabled=0;
 campaign_weapon_supply.names.count=5;rf_scene_weapon_supply[3]=123;
 for(i=0;i<5;i++){strcpy(campaign_weapon_supply.names.names[i],names[i]);campaign_weapon_supply.definitions[i]=(rf_weapon_acquire_definition){0,(int32_t)(100+i),10};}
 CHECK(!scene_checkpoint_player_catalog(&c)&&c.count==5&&c.hash==123&&c.reserve_capacity[0]==104&&c.health_capacity==100);
 for(i=0;i<5;i++)CHECK(c.supported[i]==1);for(i=5;i<64;i++)CHECK(!c.supported[i]);
 campaign_weapon_supply.names.names[4][0]=0;CHECK(scene_checkpoint_player_catalog(&c)==RF_FORMAT);
 CHECK(!transport_semantic_test());
 puts("PASS scene RFCP settled-mode, dynamic-owner and supported catalog gates");return 0;
}
