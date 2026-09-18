/* Actual authored Fighter hull and actual dry/wet levels; no host input. */
#include <stdio.h>
#include "rf/geometry.h"
#include "rf/liquid_damage.h"
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_ground_vehicle_physics.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fighter motion line%d: %s\n",__LINE__,#x);return 1;}}while(0)
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
#include "../src/diagnostic/scene_fighter_motion.inc"
int main(void)
{
    rf_vpp tables={0},meshes={0},levels={0};rf_vpp_entry entry;char *text;
    scene_driller_resources r={0};scene_driller_physics body;
    rf_model_file *file=malloc(sizeof(*file));rf_geometry g={0};rf_level level;
    rf_geometry_collision_world world={0};rf_geometry_collision_movers movers={0};rf_liquid_rooms liquids={0};
    float origin[3]={30,12,-160};const float basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i;
    CHECK(file);CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_vpp_find(&tables,"entity.tbl",&entry));text=malloc(entry.size);CHECK(text);
    CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
    CHECK(!rf_entity_assets_load("Installed_Game/tables.vpp","Fighter01","",&r.assets,2*1024*1024));
    CHECK(!rf_model_compiled_filename(r.assets.model,r.model,".v3m"));
    CHECK(!rf_entity_physics_config_load(&tables,"Fighter01",2*1024*1024,&r.physics));
    CHECK(!rf_entity_movement_load(&tables,"Fighter01",2*1024*1024,&r.movement));
    CHECK(!rf_entity_rotation_values_read(text,entry.size,"Fighter01",&r.rotation));
    CHECK(!rf_model_file_open(file,&meshes,r.model));CHECK(!rf_static_render_resource_open(file,4*1024*1024,&r.render));
    printf("FIGHTER_META use%u move%u spheres%u mass%g speed%g rotation%g\n",r.physics.authored.use_kind,r.physics.authored.movement_index,r.render.sphere_count,r.physics.authored.mass,r.movement.speed,r.rotation.maximum_velocity);
    for(i=0;i<r.render.sphere_count;i++)printf("FIGHTER_MODEL_SPHERE %s parent%d\n",r.render.spheres[i].name,r.render.spheres[i].parent);
    {int status=scene_vehicle_body_physics_initialize("Fighter01",&r.physics,&r.movement,&r.rotation,&r.render,origin,basis,0,&body);printf("FIGHTER_INIT %d\n",status);CHECK(!status);}
    CHECK(body.sphere_count==2 && !body.spring_count && body.parameters.mass==1500);
    CHECK(!rf_vpp_open(&levels,"Installed_Game/levelsm.vpp"));CHECK(!rf_level_open(&level,&levels,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&g,&level,16*1024*1024));CHECK(!rf_geometry_collision_world_open(&g,16*1024*1024,&world));
    CHECK(!rf_liquid_rooms_open(&g,65536,&liquids));
    {
        scene_fighter_motion motion={0};rf_vehicle_submersible_backend backend;
        rf_vehicle_submersible_parameters parameters={1500,20,10,4,4,2};
        rf_vehicle_submersible_command command={0};rf_vehicle_submersible_result result;
        rf_vehicle_rigid_state state=body.state;rf_vehicle_rigid_proposal proposal={0},resolved;
        uint32_t allowed=0,accepted=0,j,k;int y;
        motion.collision.world=&world;motion.collision.movers=&movers;
        motion.collision.spheres=body.collision;motion.collision.count=body.sphere_count;
        motion.collision.radius=body.radius;motion.collision.flags=4;motion.liquids=&liquids;
        CHECK(!scene_fighter_motion_backend(&motion,&backend));
        /* Candidate is not assumed valid: actual dry-room admission and all
         * authored hull spheres must clear each of six one-metre sweeps. */
        for(y=6;y<=20 && !accepted;y++){
            uint32_t blocked=0;state.position[1]=(float)y;
            memcpy(proposal.position,state.position,12);memcpy(proposal.orientation,state.orientation,36);
            CHECK(!scene_fighter_motion_air_path(&motion,&state,&proposal,&allowed));
            if(!allowed)continue;
            for(j=0;j<3&&!blocked;j++)for(k=0;k<2&&!blocked;k++){
                rf_collision_body_query q={0};rf_geometry_body_hit hit={0};uint32_t found=0;
                memcpy(q.start,state.position,12);memcpy(q.end,state.position,12);q.end[j]+=(k?1:-1);
                memcpy(q.matrix,basis,36);q.radius=body.radius;q.flags=4;q.spheres=body.collision;q.count=body.sphere_count;q.limit=1;
                CHECK(!rf_geometry_collision_body_sweep(&world,&movers,&q,NULL,0,metadata,NULL,&hit,&found));blocked=found;
            }
            if(!blocked)accepted=1;
        }
        CHECK(accepted);printf("FIGHTER_AIR_FIXTURE %g %g %g\n",state.position[0],state.position[1],state.position[2]);
        command.controlled=1;command.throttle=1;
        {float initial=state.position[2];
         for(i=0;i<20;++i)CHECK(!rf_vehicle_submersible_step(&state,&parameters,&command,1.0f/60,&backend,&result));
         CHECK(result.wet && !result.water_blocked && state.position[2]>initial+.1f);}
        /* The reusable core result.wet means air-admitted for this backend. */
        memcpy(proposal.position,state.position,12);memcpy(proposal.orientation,state.orientation,36);
        proposal.position[2]+=.25f;
        CHECK(!scene_fighter_motion_resolve(&motion,&state,&proposal,&resolved));
        proposal.position[1]=1000;
        CHECK(!scene_fighter_motion_air_path(&motion,&state,&proposal,&allowed) && !allowed);
        rf_liquid_rooms_close(&liquids);rf_geometry_collision_world_close(&world);rf_geometry_close(&g);rf_vpp_close(&levels);
        CHECK(!rf_vpp_open(&levels,"Installed_Game/levels1.vpp"));CHECK(!rf_level_open(&level,&levels,"L5S3.rfl"));
        CHECK(!rf_geometry_open(&g,&level,16*1024*1024));CHECK(!rf_geometry_collision_world_open(&g,16*1024*1024,&world));
        CHECK(!rf_liquid_rooms_open(&g,65536,&liquids));
        state.position[0]=-25;state.position[1]=-15;state.position[2]=0;
        memcpy(proposal.position,state.position,12);memcpy(proposal.orientation,state.orientation,36);
        CHECK(!scene_fighter_motion_air_path(&motion,&state,&proposal,&allowed) && !allowed);
    }
    rf_liquid_rooms_close(&liquids);rf_geometry_collision_world_close(&world);rf_geometry_close(&g);
    rf_vpp_close(&levels);rf_static_render_resource_close(&r.render);rf_vpp_close(&tables);rf_vpp_close(&meshes);free(file);free(text);
    puts("PASS actual Fighter hull dry hover translation and L5S3 submerged rejection");return 0;
}
