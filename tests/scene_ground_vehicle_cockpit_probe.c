/* Near-term Jeep cockpit admission through the same production vehicle owner. */
#include <stdio.h>
#include "../src/diagnostic/scene_driller_cockpit.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Jeep cockpit line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_cockpit *owner=NULL;
    rf_materials materials={0};rf_preview_mesh output={0};rf_level camera={0};
    float eye[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};unsigned char light[3]={200,200,200};
    const uint32_t budget=2u*1024u*1024u,draw_capacity=1024u*1024u;
    uint32_t i,faces=0;char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_vehicle_cockpit_open("jeep.vfx",&meshes,maps,4,budget,&owner));
    CHECK(owner->endpoint==4 && owner->geometry->count==1 && owner->geometry->meshes[0]->prefix.faces==620);
    CHECK(!strcmp(owner->geometry->meshes[0]->prefix.parent,"Scene Root"));
    CHECK(owner->materials->count==3 && owner->resident_bytes<=budget && owner->peak_bytes<=budget);
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    CHECK(!scene_driller_cockpit_merge(owner,&materials,256));output.vertices=calloc(1,draw_capacity);CHECK(output.vertices);
    camera.player_orientation[0][0]=camera.player_orientation[1][1]=camera.player_orientation[2][2]=1;
    CHECK(!scene_driller_cockpit_draw(owner,.125f,eye,basis,&camera,&output,draw_capacity,materials.count,light,&faces));
    CHECK(faces && output.count);for(i=0;i<output.count;i++)CHECK(output.vertices[i].material<materials.count);
    printf("JEEP_COCKPIT resident=%u peak=%u textures=%u imageframes=%u visible_faces=%u vertices=%u drawbytes=%u\n",
        owner->resident_bytes,owner->peak_bytes,owner->materials->textures.texture_count,materials.count,faces,output.count,output.bytes);
    scene_driller_cockpit_close(&owner);rf_materials_close(&materials);free(output.vertices);return 0;
}
