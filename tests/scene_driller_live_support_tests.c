#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_vehicle_collision.inc"
#include "../src/diagnostic/scene_driller_physics.inc"
#include "../src/diagnostic/scene_driller_support.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller live support line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct live_fixture {scene_vehicle_collision collision;scene_driller_support_context support;uint32_t queries,hits;} live_fixture;
static int plane_sphere(void *opaque,const rf_collision_body_request *q,rf_collision_body_candidate *out,uint32_t *found)
{
    const float plane[4]={0,1,0,0};float time=0,point[3];uint32_t hit;int status;(void)opaque;
    status=rf_collision_sphere_plane(q->start,q->delta,q->radius,plane,&time,point,&hit);if(status)return status;
    *found=hit && time<=q->limit;
    if(*found){memset(out,0,sizeof(*out));out->hit.fraction=time;memcpy(out->hit.point,point,12);memcpy(out->hit.normal,plane,12);out->material=0;}
    return RF_OK;
}
static int plane_query(void *opaque,const rf_collision_body_query *q,rf_geometry_body_hit *hit,uint32_t *found)
{
    live_fixture *f=opaque;int status;++f->queries;
    status=rf_collision_body_sweep(q,NULL,0,plane_sphere,NULL,&hit->contact,found);
    hit->solid=UINT32_MAX;if(!status&&*found)++f->hits;return status;
}
static int support_callback(void *opaque,const rf_vehicle_rigid_state *s,rf_vehicle_rigid_support *out)
{return scene_driller_support_sample(&((live_fixture *)opaque)->support,s,out);}
static int resolve_callback(void *opaque,const rf_vehicle_rigid_state *s,const rf_vehicle_rigid_proposal *p,rf_vehicle_rigid_proposal *out)
{return scene_vehicle_collision_resolve(&((live_fixture *)opaque)->collision,s,p,out);}
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}},tables={0};scene_driller_resources *resource=NULL;scene_driller_physics physics;
    static scene_stream stream;static rf_geometry_collision_world world;static rf_surface_materials palette;
    rf_vpp_entry entry;void *text;live_fixture fixture={0};rf_vehicle_rigid_backend backend={&fixture,support_callback,resolve_callback};
    rf_vehicle_rigid_command command={0,0,1};float position[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};
    float start_height=0,low=1e30f,high=-1e30f,min_up=1;uint32_t i,j,support_frames=0;char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resource));
    CHECK(!scene_driller_physics_initialize(resource,position,basis,scene_gravity.acceleration,&physics));
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_find(&tables,"materials.tbl",&entry));
    text=malloc(entry.size);CHECK(text);CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
    CHECK(!rf_surface_materials_read(text,entry.size,&palette));free(text);rf_vpp_close(&tables);campaign_surface_palette=&palette;
    for(i=0;i<physics.sphere_count;i++)start_height=fmaxf(start_height,physics.spheres[i].radius-physics.spheres[i].center[1]);
    physics.state.position[1]=start_height+.1f;stream.collision=&world;
    fixture.collision.world=&world;fixture.collision.movers=&campaign_movers;
    fixture.collision.spheres=physics.collision;fixture.collision.count=physics.sphere_count;fixture.collision.radius=physics.radius;fixture.collision.flags=4;
    fixture.collision.query=plane_query;fixture.collision.query_context=&fixture;
    CHECK(!scene_driller_support_initialize(&fixture.support,&stream,&fixture.collision,&physics));
    for(i=0;i<200;i++){
        command.throttle=i>=120?1:0;
        CHECK(!rf_vehicle_rigid_step(&physics.state,&physics.parameters,&command,1.f/60,&backend));
        if(i>=60){low=fminf(low,physics.state.position[1]);high=fmaxf(high,physics.state.position[1]);}
        min_up=fminf(min_up,physics.state.orientation[4]);support_frames+=fixture.support.hits>0;
        for(j=0;j<physics.sphere_count;j++){
            float y=physics.state.position[1];uint32_t k;
            for(k=0;k<3;k++)y+=physics.collision[j].center[k]*physics.state.orientation[k*3+1];
            CHECK(y-physics.collision[j].radius>-.02f);
        }
    }
    printf("DRILLER_SETTLE gravity%g mass%g startY%g final(%.6g,%.6g,%.6g) heightRange%g minUp%g supportFrames%u queries%u hits%u\n",
        scene_gravity.acceleration,physics.parameters.mass,start_height+.1f,physics.state.position[0],physics.state.position[1],physics.state.position[2],high-low,min_up,support_frames,fixture.queries,fixture.hits);
    CHECK(support_frames>60 && high-low<.5f && min_up>.85f && physics.state.position[2]>.1f);
    scene_driller_resources_close(&resource);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    campaign_surface_palette=NULL;
    puts("actual authored Driller200frame settle/drive on geometric plane; live level/moving support not exercised");return 0;
}
