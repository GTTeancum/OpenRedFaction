#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene_driller_resources.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller resources line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_resources *o=NULL,*failed=NULL;
    float position[3]={10,20,30},basis[9]={1,0,0,0,1,0,0,0,1},pose[12];
    uint32_t i,j,vertices=0,triangles=0,max_batch=0;char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,32768,&failed)==RF_RANGE && !failed);
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&o));
    CHECK(!strcmp(o->assets.model,"Driller01.v3d") && !strcmp(o->model,"Driller01.v3m"));
    CHECK(o->physics.authored.use_kind==1 && o->physics.authored.use_radius==10 && o->physics.authored.movement_index==5);
    CHECK(o->movement.speed==6 && o->movement.acceleration==3 && o->render.part_count && o->materials.count);
    CHECK(o->rotation.maximum_velocity==1.2f && o->rotation.acceleration==2);
    CHECK(o->seat==19 && o->tags.count==32 && !strcmp(o->tags.items[o->seat].name,"interface_1"));
    for(i=0;i<o->render.part_count;i++) {
        uint32_t li=o->render.parts[i].first_lod;const rf_model_geometry *g;
        CHECK(li<o->render.lod_count);g=&o->render.lods[li].geometry;
        vertices+=g->vertex_count;triangles+=g->triangle_count;
        for(j=0;j<g->batch_count;j++) {
            uint32_t slot;const rf_model_draw_batch *b=g->batches+j;
            if(b->vertices>max_batch)max_batch=b->vertices;
            if(b->material==UINT32_MAX)continue;
            CHECK(b->material<o->materials.count);memcpy(&slot,o->materials.items[b->material].record.bytes+0x10,4);
            CHECK(slot<o->materials.textures.count && o->materials.textures.items[slot].image.rgba);
        }
    }
    printf("DRILLER_RESOURCE resident=%u peak=%u geometry=%u materials=%u tags=%u parts=%u lods=%u vertices=%u triangles=%u max_batch=%u health=%g armor=%g\n",
        o->resident_bytes,o->peak_bytes,o->render.allocated_bytes,o->materials.resident_bytes,o->tags.allocated_bytes,
        o->render.part_count,o->render.lod_count,vertices,triangles,max_batch,o->vitals.health,o->vitals.armor);
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    CHECK(!scene_driller_seat_pose(o,position,basis,pose));
    for(i=0;i<3;i++)CHECK(fabsf(pose[9+i]-(position[i]+o->tags.items[o->seat].position[i]))<.00001f);
    scene_driller_resources_close(&o);scene_driller_resources_close(&o);CHECK(!o);return 0;
}
