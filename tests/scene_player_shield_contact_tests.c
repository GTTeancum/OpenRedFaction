#include "../src/diagnostic/scene_player_shield_contact.inc"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(int argc,char **argv)
{
    static rf_player_weapon w;rf_model_vertex vertices[6]={0};int32_t reuse[6]={0};
    rf_model_triangle triangles[2]={{{0,1,2},32},{{3,4,5},32}};
    rf_model_draw_batch batch={0,6,0,2,0};rf_collision_model_response_hit hit={0};
    float camera[3]={0},eye[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};
    float start[3]={0,0,2},end[3]={0,0,-2};uint32_t i,accepted;
    w.initialized=1;w.bone_count=2;strcpy(w.bones[0].name,"hand");w.bones[0].parent=-1;
    strcpy(w.bones[1].name,"fk-shield");w.bones[1].parent=0;
    for(i=0;i<2;++i)w.prepared[i][0]=w.prepared[i][4]=w.prepared[i][8]=256.f/255.f;
    for(i=0;i<6;++i){vertices[i].weights[0]=255;vertices[i].bones[0]=i<3?1:0;vertices[i].position[2]=i<3?0:1;}
    vertices[0].position[0]=vertices[3].position[0]=-1;vertices[0].position[1]=vertices[3].position[1]=-1;
    vertices[1].position[0]=vertices[4].position[0]=1;vertices[1].position[1]=vertices[4].position[1]=-1;
    vertices[2].position[1]=vertices[5].position[1]=1;
    w.geometry=(rf_model_geometry){&batch,vertices,triangles,reuse,1,6,2,0};
    assert(scene_player_shield_contact(&w,camera,eye,basis,start,end,1,&hit,&accepted)==RF_OK && accepted && fabsf(hit.time-.5f)<.00001f);
    /* Nearer hand face at z1 must never intercept. Mixed hand influence excludes
     * the shield triangle too, leaving no eligible collision surface. */
    vertices[0].weights[1]=1;vertices[0].bones[1]=0;
    assert(scene_player_shield_contact(&w,camera,eye,basis,start,end,1,&hit,&accepted)==RF_OK && !accepted);
    vertices[0].weights[1]=0;
    assert(scene_player_shield_contact(&w,camera,eye,basis,start,end,.5f,&hit,&accepted)==RF_OK && !accepted);
    assert(scene_player_shield_contact(&w,camera,eye,basis,end,start,1,&hit,&accepted)==RF_OK && accepted);
    triangles[0].flags=0;
    assert(scene_player_shield_contact(&w,camera,eye,basis,end,start,1,&hit,&accepted)==RF_OK && !accepted);
    triangles[0].flags=32;
    memset(basis,0,sizeof(basis));basis[2]=-1;basis[4]=1;basis[6]=1;eye[0]=10;camera[2]=.25f;
    start[0]=12;end[0]=8;start[2]=end[2]=0;
    assert(scene_player_shield_contact(&w,camera,eye,basis,start,end,1,&hit,&accepted)==RF_OK && accepted && fabsf(hit.point[0]-9.75f)<.00001f);
    if(argc>1){
        rf_vpp archive={0};rf_model_file model;uint8_t raw[4+50*56],selected[50];uint32_t n,b,t,count=0;
        memset(&w,0,sizeof(w));assert(rf_vpp_open(&archive,argv[1])==RF_OK);
        assert(rf_model_file_open(&model,&archive,"fp_riotshield.v3c")==RF_OK);
        for(n=0;n<model.section_count;++n)if(model.sections[n].type==0x424f4e45){
            assert(model.sections[n].size<=sizeof(raw));assert(rf_vpp_read(&archive,&model.entry,model.sections[n].offset,raw,model.sections[n].size)==RF_OK);
            assert(rf_model_decode_bones(raw,model.sections[n].size,w.bones,50,&w.bone_count)==RF_OK);
        }
        assert(w.bone_count==20 && scene_player_shield_bones(&w,selected)==RF_OK && selected[19]);
        assert(rf_model_geometry_open(&w.geometry,&model,0,128*1024)==RF_OK);
        for(n=0;n<w.bone_count;++n)w.prepared[n][0]=w.prepared[n][4]=w.prepared[n][8]=1;
        for(b=0;b<w.geometry.batch_count;++b)for(t=0;t<w.geometry.batches[b].triangles;++t){
            rf_collision_model_triangle tri;uint32_t eligible;const rf_model_draw_batch *draw=w.geometry.batches+b;
            assert(scene_player_shield_triangle(&w,draw,w.geometry.triangles+draw->first_triangle+t,selected,&tri,&eligible)==RF_OK);
            if(eligible){assert(b==3 || b==4);++count;}
        }
        assert(count==256);printf("authored FP shield: %u physical triangles, arms excluded\n",count);
        rf_model_geometry_close(&w.geometry);rf_vpp_close(&archive);
    }
    return 0;
}
