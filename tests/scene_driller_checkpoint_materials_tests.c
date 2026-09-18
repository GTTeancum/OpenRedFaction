/* Actual committed drill cutters exercise the scene RFDS material adapter. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller materials line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp levels={0};rf_level level={0};rf_geometry geometry={0};
    rf_geomod_authored_post *owner=NULL;rf_geomod_authored_post_view source;
    rf_geomod_terrain *terrain=NULL;static rf_geomod_template shape;
    rf_collision_face_filter generated={0};unsigned char *wire,*original;
    unsigned char convex[28+24+60*20+21*16]={0};uint32_t bytes,i,n,at;
    const char *names[2]={"build/data/driller-single.bin","build/data/driller-double.bin"};
    float center[3]={41.3125f,6.73709393f,-167.045395f},basis[9]={0,0,-1,0,1,0,1,0,0};
    CHECK(!rf_vpp_open(&levels,"Installed_Game/levelsm.vpp"));CHECK(!rf_level_open(&level,&levels,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_cavity_open_source(&level,&geometry,148,2*1024*1024,&owner));
    CHECK(!rf_geomod_authored_post_get(owner,&source));generated.face_flags=256;
    for(i=0;i<2;i++){
        /* The recorded wall/floor corner belongs to the double cutter. Keep
         * the single cutter clear of that corner for its separate fixture. */
        center[1]=i?6.73709393f:7.5f;
        CHECK(!rf_geomod_template_load(names[i],&shape));
        CHECK(!rf_geomod_terrain_open(&source.source,source.source_filters,&generated,1,4096,768,2*1024*1024,&terrain));
        {int status=rf_geomod_terrain_cut_template_scale(terrain,&shape,center,basis,1,999);
            if(status)fprintf(stderr,"actual cutter %s faces=%u status=%d\n",names[i],shape.face_count,status);
            CHECK(!status);}
        CHECK(!rf_geomod_terrain_history_size(terrain,&bytes));wire=malloc(bytes);original=malloc(bytes);CHECK(wire && original);
        CHECK(!rf_geomod_terrain_history_encode(terrain,wire,bytes));memcpy(original,wire,bytes);
        CHECK(checkpoint_u32(wire+12)==1 && checkpoint_u32(wire+28)==1);
        CHECK(checkpoint_u32(wire+36)>20 && checkpoint_u32(wire+36)<=64);
        if(i==1)CHECK(checkpoint_u32(wire+36)==64);
        CHECK(!scene_checkpoint_materials_mode(wire,bytes,999,0,0));CHECK(!memcmp(wire,original,bytes));
        CHECK(!scene_checkpoint_materials(wire,bytes,999,0));CHECK(!scene_checkpoint_materials_mode(wire,bytes,0,0,0));
        CHECK(!scene_checkpoint_materials(wire,bytes,0,999));CHECK(!memcmp(wire,original,bytes));
        printf("DRILLER_MATERIAL_ROUNDTRIP %s vertices=%u faces=%u bytes=%u\n",names[i],checkpoint_u32(wire+32),checkpoint_u32(wire+36),bytes);
        free(original);free(wire);rf_geomod_terrain_close(&terrain);
    }
    /* Material parser only: keep the legacy convex60-vertex/20-face ceiling,
     * independently of the expanded star limits. No geometric claim here. */
    memcpy(convex,"RGCH",4);checkpoint_put(convex+4,1);checkpoint_put(convex+12,1);
    checkpoint_put(convex+32,60);checkpoint_put(convex+36,20);n=sizeof(convex)-16;checkpoint_put(convex+8,n);
    CHECK(!scene_checkpoint_materials_mode(convex,n,0,0,0));
    checkpoint_put(convex+36,21);checkpoint_put(convex+8,sizeof(convex));
    CHECK(scene_checkpoint_materials_mode(convex,sizeof(convex),0,0,0)==RF_FORMAT);
    checkpoint_put(convex+28,2);CHECK(scene_checkpoint_materials_mode(convex,sizeof(convex),0,0,0)==RF_FORMAT);
    /* No adapter mutation occurred during the rejected validation calls. */
    for(at=52;at<sizeof(convex);at++)CHECK(!convex[at]);
    rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&levels);
    puts("PASS actual single/double drill RGCH material roundtrip and convex20-face cap");return 0;
}
