/* Actual authored submarine hull and liquid-room placement; no host input. */
#include <stdio.h>
#include "rf/geometry.h"
#include "rf/liquid_damage.h"
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_ground_vehicle_physics.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sub world line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int metadata(void *c,uint32_t a,uint32_t b,uint32_t *t,uint32_t *m)
{(void)c;(void)a;(void)b;*t=*m=0;return RF_OK;}
int main(void)
{
    rf_vpp tables={0},meshes={0},levels={0};rf_vpp_entry entry;char *text;
    scene_driller_resources r={0};scene_driller_physics body;
    rf_model_file *file=malloc(sizeof(*file));rf_geometry g={0};rf_level level;
    rf_geometry_collision_world world={0};rf_geometry_collision_movers movers={0};rf_liquid_rooms liquids={0};
    float origin[3]={0,-30,0};const float basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,j,k,blocked=0;
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
    {int x,y,z,accepted=0;
     for(x=-25;x<=45&&!accepted;x+=5)for(y=-55;y<=5&&!accepted;y+=5)for(z=-15;z<=15&&!accepted;z+=5){
        rf_collision_room_location room;float center[3]={(float)x,(float)y,(float)z};uint32_t obstruction=0;
        CHECK(!rf_geometry_collision_world_locate(&world,center,&room));
        if(room.room>=liquids.count || liquids.items[room.room].type!=1 || !isfinite(liquids.items[room.room].depth) ||
           center[1]+body.radius>=liquids.items[room.room].minimum_y+liquids.items[room.room].depth)continue;
        for(j=0;j<3&&!obstruction;j++)for(k=0;k<2&&!obstruction;k++){
            rf_collision_body_query q={0};rf_geometry_body_hit hit={0};uint32_t found=0;
            memcpy(q.start,center,12);memcpy(q.end,center,12);q.end[j]+=(k?1:-1)*2;
            memcpy(q.matrix,basis,36);q.radius=body.radius;q.flags=4;q.spheres=body.collision;q.count=body.sphere_count;q.limit=1;
            CHECK(!rf_geometry_collision_body_sweep(&world,&movers,&q,NULL,0,metadata,NULL,&hit,&found));obstruction|=found;
        }
        if(!obstruction){memcpy(origin,center,12);accepted=1;printf("SUB_FIXTURE %g %g %g room%u\n",origin[0],origin[1],origin[2],room.room);}
     }
     CHECK(accepted);}
    for(i=0;i<body.sphere_count;i++){
        float center[3];rf_collision_room_location room;const rf_liquid_room *wet;
        for(j=0;j<3;j++)center[j]=origin[j]+body.spheres[i].center[j];
        CHECK(!rf_geometry_collision_world_locate(&world,center,&room));CHECK(room.room<liquids.count);
        wet=liquids.items+room.room;CHECK(wet->type==1 && isfinite(wet->depth));
        CHECK(center[1]+body.spheres[i].radius<wet->minimum_y+wet->depth);
        printf("SUB_WATER sphere%u room%u center%g,%g,%g radius%g surface%g\n",i,room.room,center[0],center[1],center[2],body.spheres[i].radius,wet->minimum_y+wet->depth);
    }
    for(j=0;j<3;j++)for(k=0;k<2;k++){
        rf_collision_body_query q={0};rf_geometry_body_hit hit={0};uint32_t found=0;
        memcpy(q.start,origin,12);memcpy(q.end,origin,12);q.end[j]+=(k?1:-1)*2;
        memcpy(q.matrix,basis,36);q.radius=body.radius;q.flags=4;q.spheres=body.collision;q.count=body.sphere_count;q.limit=1;
        CHECK(!rf_geometry_collision_body_sweep(&world,&movers,&q,NULL,0,metadata,NULL,&hit,&found));blocked+=found;
        printf("SUB_CLEAR axis%u sign%u blocked%u fraction%g\n",j,k,found,hit.contact.fraction);
    }
    CHECK(!blocked);
    rf_liquid_rooms_close(&liquids);rf_geometry_collision_world_close(&world);rf_geometry_close(&g);
    rf_vpp_close(&levels);rf_static_render_resource_close(&r.render);rf_vpp_close(&tables);rf_vpp_close(&meshes);free(file);free(text);
    puts("PASS authored sub hull/inertia and six two-metre wet-room clearance sweeps");return 0;
}
