#include <stdio.h>
#include "../src/diagnostic/scene_driller_cockpit.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller cockpit line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_cockpit *o=NULL,*failed=NULL;
    rf_materials materials={0};rf_preview_mesh output={0};rf_level camera={0};
    float eye[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};unsigned char light[3]={200,200,200};
    uint32_t i,j,faces=0,total=0,frames=0,capacity=300000,animated=0;char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(scene_driller_cockpit_open(&meshes,maps,4,32768,&failed)==RF_RANGE && !failed);
    CHECK(!scene_driller_cockpit_open(&meshes,maps,4,2*1024*1024,&o));
    CHECK(o->endpoint==26 && o->geometry->count==6);
    for(i=0;i<o->geometry->count;i++)total+=o->geometry->meshes[i]->prefix.faces;
    CHECK(total==226);
    for(i=0;i<o->materials->textures.texture_count;i++){
        rf_particle_animation *a=&o->materials->textures.textures[i].animation;frames+=a->count;
        if(a->count==2)animated++;
        for(j=0;j<a->count;j++)CHECK(a->images[j].rgba);
    }
    CHECK(animated==1);
    CHECK(!scene_driller_cockpit_merge(o,&materials,256));CHECK(materials.count==frames);
    output.vertices=calloc(1,capacity);CHECK(output.vertices);
    camera.player_orientation[0][0]=camera.player_orientation[1][1]=camera.player_orientation[2][2]=1;
    CHECK(!scene_driller_cockpit_draw(o,.125f,eye,basis,&camera,&output,capacity,materials.count,light,&faces));
    CHECK(faces && output.count);
    for(i=0;i<output.count;i++)CHECK(output.vertices[i].material<materials.count && isfinite(output.vertices[i].position[0]));
    printf("DRILLER_COCKPIT resident=%u peak=%u source_faces=%u visible_faces=%u vertices=%u textures=%u frames=%u\n",
        o->resident_bytes,o->peak_bytes,total,faces,output.count,o->materials->textures.texture_count,frames);
    /* Eye translation moves both parent and camera, preserving exact local projection. */
    {float x=output.vertices[0].position[0];uint32_t n=output.count;
     eye[0]=camera.player_position[0]=20;output.count=output.bytes=0;
     CHECK(!scene_driller_cockpit_draw(o,.125f,eye,basis,&camera,&output,capacity,materials.count,light,&faces));
     CHECK(output.count==n && output.vertices[0].position[0]==x);}
    scene_driller_cockpit_close(&o);rf_materials_close(&materials);free(output.vertices);
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);return 0;
}
