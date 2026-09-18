#include "../src/diagnostic/scene_riot_shield_contact.inc"
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv)
{
    rf_model_vertex vertices[3]={0};int32_t reuse[3]={0};
    rf_model_triangle triangle={{0,1,2},0x20};
    rf_model_draw_batch batch={0,3,0,1,0};rf_model_part_metadata part={0};
    rf_static_render_lod lod={0};rf_static_render_resource model={0};
    rf_weapon_hand_placement pose={0};rf_collision_model_response_hit hit={0};uint32_t accepted;
    float start[3]={0,0,2},end[3]={0,0,-2};
    vertices[0].position[0]=-1;vertices[0].position[1]=-1;
    vertices[1].position[0]=1;vertices[1].position[1]=-1;
    vertices[2].position[1]=1;
    lod.geometry.batches=&batch;lod.geometry.batch_count=1;lod.geometry.vertices=vertices;
    lod.geometry.vertex_count=3;lod.geometry.triangles=&triangle;lod.geometry.triangle_count=1;lod.geometry.reuse=reuse;
    model.parts=&part;model.part_count=1;model.lods=&lod;model.lod_count=1;
    pose.basis[0]=pose.basis[4]=pose.basis[8]=1;
    assert(scene_riot_shield_contact(&model,&pose,start,end,.75f,&hit,&accepted)==RF_OK && accepted && hit.time==.5f);
    assert(hit.point[2]==0 && hit.normal[2]==1);
    assert(scene_riot_shield_contact(&model,&pose,start,end,.5f,&hit,&accepted)==RF_OK && !accepted);
    assert(scene_riot_shield_contact(&model,&pose,start,end,.25f,&hit,&accepted)==RF_OK && !accepted);
    assert(scene_riot_shield_contact(&model,&pose,end,start,.75f,&hit,&accepted)==RF_OK && accepted && hit.normal[2]==-1);
    triangle.flags=0;
    assert(scene_riot_shield_contact(&model,&pose,end,start,.75f,&hit,&accepted)==RF_OK && !accepted);
    start[0]=end[0]=2;
    assert(scene_riot_shield_contact(&model,&pose,start,end,1,&hit,&accepted)==RF_OK && !accepted);
    /* Rotate local +z into world +x and translate center to x=10. */
    memset(pose.basis,0,sizeof(pose.basis));pose.basis[2]=-1;pose.basis[4]=1;pose.basis[6]=1;pose.position[0]=10;
    start[0]=12;end[0]=8;start[2]=end[2]=0;
    assert(scene_riot_shield_contact(&model,&pose,start,end,.75f,&hit,&accepted)==RF_OK && accepted);
    assert(hit.point[0]==10 && hit.normal[0]==1 && hit.time==.5f);
    /* Optional actual authored resource ownership check; parent supplies
     * Installed_Game/meshes.vpp. This does not claim live held placement. */
    if(argc>1){
        rf_vpp archive={0};rf_model_file file;rf_static_render_resource actual={0};
        assert(rf_vpp_open(&archive,argv[1])==RF_OK);
        assert(rf_model_file_open(&file,&archive,"weapon_riotshield.v3m")==RF_OK);
        assert(rf_static_render_resource_open(&file,128*1024,&actual)==RF_OK);
        assert(actual.part_count>0 && actual.allocated_bytes<=128*1024);
        printf("authored shield retained bytes: %u\n",actual.allocated_bytes);
        rf_static_render_resource_close(&actual);rf_vpp_close(&archive);
    }
    return 0;
}
