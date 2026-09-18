/* Actual authored submarine hull and liquid-room placement; no host input. */
#include <stdio.h>
#include "rf/geometry.h"
#include "rf/liquid_damage.h"
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_ground_vehicle_physics.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sub world line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int metadata(void *c,uint32_t a,uint32_t b,uint32_t *t,uint32_t *m)
{(void)c;(void)a;(void)b;*t=*m=0;return RF_OK;}
/* Real collision gateway without scene material/damage side effects. */
static int campaign_body_query_batch_for(const rf_geometry_collision_world *world,
    const rf_collision_body_query *query,rf_geometry_body_hit *hit,uint32_t *found,
    rf_collision_sweep_batch *batch,const rf_geometry_collision_movers *movers)
{
    (void)batch;
    return rf_geometry_collision_body_sweep(world,movers,query,NULL,0,metadata,NULL,hit,found);
}
#include "../src/diagnostic/scene_vehicle_collision.inc"
#include "../src/diagnostic/scene_submarine_motion.inc"
int main(void)
{
    rf_vpp tables={0},meshes={0},levels={0};rf_vpp_entry entry;char *text;
    scene_driller_resources r={0};scene_driller_physics body;
    rf_model_file *file=malloc(sizeof(*file));rf_geometry g={0};rf_level level;
    rf_geometry_collision_world world={0};rf_geometry_collision_movers movers={0};rf_liquid_rooms liquids={0};
    float origin[3]={-25,-15,0};const float basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i;
    CHECK(file);CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_vpp_find(&tables,"entity.tbl",&entry));text=malloc(entry.size);CHECK(text);
    CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
    CHECK(!rf_entity_assets_load("Installed_Game/tables.vpp","sub","",&r.assets,2*1024*1024));
    CHECK(!rf_model_compiled_filename(r.assets.model,r.model,".v3m"));
    CHECK(!rf_entity_physics_config_load(&tables,"sub",2*1024*1024,&r.physics));
    CHECK(!rf_entity_movement_load(&tables,"sub",2*1024*1024,&r.movement));
    CHECK(!rf_entity_rotation_values_read(text,entry.size,"sub",&r.rotation));
    CHECK(!rf_model_file_open(file,&meshes,r.model));CHECK(!rf_static_render_resource_open(file,4*1024*1024,&r.render));
    printf("SUB_META use%u move%u spheres%u mass%g speed%g rotation%g\n",r.physics.authored.use_kind,r.physics.authored.movement_index,r.render.sphere_count,r.physics.authored.mass,r.movement.speed,r.rotation.maximum_velocity);
    for(i=0;i<r.render.sphere_count;i++)printf("SUB_MODEL_SPHERE %s parent%d\n",r.render.spheres[i].name,r.render.spheres[i].parent);
    {int status=scene_vehicle_body_physics_initialize("sub",&r.physics,&r.movement,&r.rotation,&r.render,origin,basis,0,&body);printf("SUB_INIT %d\n",status);CHECK(!status);}
    CHECK(body.sphere_count==1 && !body.spring_count && body.parameters.mass==2500);
    CHECK(!rf_vpp_open(&levels,"Installed_Game/levels1.vpp"));CHECK(!rf_level_open(&level,&levels,"L5S3.rfl"));
    CHECK(!rf_geometry_open(&g,&level,16*1024*1024));CHECK(!rf_geometry_collision_world_open(&g,16*1024*1024,&world));
    CHECK(!rf_liquid_rooms_open(&g,65536,&liquids));
    {
        scene_submarine_motion motion={0};rf_vehicle_submersible_backend backend;
        rf_vehicle_submersible_parameters parameters={2500,6,8,4,2,2};
        rf_vehicle_submersible_command command={0};rf_vehicle_submersible_result result;
        rf_vehicle_rigid_state state=body.state,before;rf_vehicle_rigid_proposal proposal={0},resolved;
        uint32_t allowed=0;float destination[3];
        motion.collision.world=&world;motion.collision.movers=&movers;
        motion.collision.spheres=body.collision;motion.collision.count=body.sphere_count;
        motion.collision.radius=body.radius;motion.collision.flags=4;motion.liquids=&liquids;
        CHECK(!scene_submarine_motion_backend(&motion,&backend));
        memcpy(proposal.position,state.position,12);memcpy(proposal.orientation,state.orientation,36);
        CHECK(!scene_submarine_motion_water_path(&motion,&state,&proposal,&allowed) && allowed);
        proposal.position[2]+=2;
        CHECK(!scene_submarine_motion_water_path(&motion,&state,&proposal,&allowed) && allowed);
        CHECK(!scene_submarine_motion_resolve(&motion,&state,&proposal,&resolved));
        CHECK(fabsf(resolved.position[2]-proposal.position[2])<.001f);
        /* Actual controller through the actual loaded hull/world/liquid adapters. */
        command.controlled=1;command.throttle=1;
        for(i=0;i<30;++i)CHECK(!rf_vehicle_submersible_step(&state,&parameters,&command,1.0f/60,&backend,&result));
        CHECK(result.wet && !result.water_blocked && state.position[2]>.5f);
        /* Preserve authored radius: center near water surface is insufficient. */
        memcpy(destination,origin,12);
        {rf_collision_room_location room;CHECK(!rf_geometry_collision_world_locate(&world,origin,&room));
         CHECK(room.room<liquids.count);destination[1]=liquids.items[room.room].minimum_y+liquids.items[room.room].depth-.1f;}
        CHECK(!scene_submarine_water_body_path(&motion,body.collision,body.sphere_count,
            origin,basis,destination,basis,&allowed) && !allowed);
        /* A dry/outside endpoint cannot turn this controller into free flight. */
        destination[1]=1000;
        CHECK(!scene_submarine_water_body_path(&motion,body.collision,body.sphere_count,
            origin,basis,destination,basis,&allowed) && !allowed);
        before=state;state.position[1]=1000;
        CHECK(!rf_vehicle_submersible_step(&state,&parameters,&command,1.0f/60,&backend,&result));
        CHECK(!result.wet && result.water_blocked && state.position[1]==1000 && state.velocity[2]==0);
        CHECK(before.position[2]>.5f);
    }
    rf_liquid_rooms_close(&liquids);rf_geometry_collision_world_close(&world);rf_geometry_close(&g);
    rf_vpp_close(&levels);rf_static_render_resource_close(&r.render);rf_vpp_close(&tables);rf_vpp_close(&meshes);free(file);free(text);
    puts("PASS actual L5S3 submarine wet motion, hull surface clearance and dry rejection");return 0;
}
