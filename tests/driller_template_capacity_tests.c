#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller template capacity line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_geomod_vertex original_vertices[24];
static rf_geomod_face original_faces[6];
static rf_collision_face_filter filters[6],generated;
static void source_box(rf_geomod_mesh_view *source)
{
    uint32_t axis,side,j;const int u[4]={-1,1,1,-1},v[4]={-1,-1,1,1};
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++){
        uint32_t f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        original_faces[f]=(rf_geomod_face){f*4,4,2,f};filters[f].face_flags=256;
        for(j=0;j<4;j++){uint32_t k=side?j:3-j;rf_geomod_vertex *p=original_vertices+f*4+j;
            p->position[axis]=side?10:-10;p->position[a]=(float)u[k]*10;p->position[b]=(float)v[k]*10;}}
    generated.face_flags=256;*source=(rf_geomod_mesh_view){original_vertices,original_faces,24,6,1};
}
int main(int argc,char **argv)
{
    const char *single=argc>1?argv[1]:"build/data/driller-single.bin";
    const char *dual=argc>2?argv[2]:"build/data/driller-double.bin";
    const char *legacy=argc>3?argv[3]:"build/data/geomod-template.bin";
    static rf_geomod_template shapes[3],saved;
    rf_geomod_mesh_view source,cut;rf_geomod_terrain *live=NULL,*copy=NULL;rf_geomod_terrain_view a,b;
    float basis[9]={1,0,0,0,1,0,0,0,1},kernel[3],minimum[3],maximum[3];uint32_t star,n,again,i;
    unsigned char *wire,*roundtrip;unsigned char bad[28+65*60]={0};FILE *file;
    CHECK(RF_GEOMOD_CUT_LIMIT==8 && RF_GEOMOD_STAR_FACE_LIMIT==64 && RF_GEOMOD_STAR_VERTEX_LIMIT==192);
    CHECK(!rf_geomod_template_load(single,shapes) && shapes[0].face_count==26);
    CHECK(!rf_geomod_template_load(dual,shapes+1) && shapes[1].face_count==64);
    CHECK(!rf_geomod_template_load(legacy,shapes+2) && shapes[2].face_count<=20);
    saved=shapes[1];file=fopen(dual,"rb");CHECK(file);CHECK(fread(bad,1,3868,file)==3868);CHECK(!fclose(file));
    bad[8]=65;CHECK(rf_geomod_template_decode(bad,sizeof(bad),shapes+1)==RF_FORMAT && !memcmp(&saved,shapes+1,sizeof(saved)));
    source_box(&source);
    CHECK(!rf_geomod_terrain_open(&source,filters,&generated,0,4096,768,1024*1024,&live));
    CHECK(!rf_geomod_terrain_open(&source,filters,&generated,0,4096,768,1024*1024,&copy));
    /* Original version1 box record remains byte-identical through import. */
    CHECK(!rf_geomod_terrain_cut_box(live,(float[3]){0,9,0},(float[3]){1,1,1},3));
    CHECK(!rf_geomod_terrain_history_size(live,&n));wire=malloc(RF_GEOMOD_HISTORY_MAX_BYTES);roundtrip=malloc(RF_GEOMOD_HISTORY_MAX_BYTES);CHECK(wire && roundtrip);
    CHECK(!rf_geomod_terrain_history_encode(live,wire,n));CHECK(!rf_geomod_terrain_history_decode(copy,wire,n));
    CHECK(!rf_geomod_terrain_history_encode(copy,roundtrip,n) && !memcmp(wire,roundtrip,n));
    for(i=0;i<2;++i){float center[3]={i?-9:9,0,0};
        CHECK(!rf_geomod_template_bounds(shapes+i,center,basis,1,NULL,0,minimum,maximum));
        CHECK(minimum[0]<center[0] && maximum[0]>center[0]);
        CHECK(!rf_geomod_terrain_cut_template_scale(live,shapes+i,center,basis,1,3));
        CHECK(!rf_geomod_terrain_cutter_get(live,i+1,&cut,kernel,&star) && star && cut.face_count==shapes[i].face_count && cut.vertex_count==shapes[i].face_count*3);
        /* Expanded STAR capacity must not silently expand convex admission. */
        CHECK(rf_geomod_terrain_cut_convex(copy,&cut)==RF_RANGE);
    }
    CHECK(!rf_geomod_terrain_get(live,&a) && a.cuts==3 && a.mesh.face_count>source.face_count);
    CHECK(!rf_geomod_terrain_history_size(live,&n) && n<=RF_GEOMOD_HISTORY_MAX_BYTES);
    CHECK(!rf_geomod_terrain_history_encode(live,wire,n));CHECK(!memcmp(wire,"RGCH\1\0\0\0",8));
    CHECK(!rf_geomod_terrain_history_decode(copy,wire,n));CHECK(!rf_geomod_terrain_history_size(copy,&again) && again==n);
    CHECK(!rf_geomod_terrain_history_encode(copy,roundtrip,n) && !memcmp(wire,roundtrip,n));
    CHECK(!rf_geomod_terrain_get(copy,&b) && b.cuts==3 && a.mesh.face_count==b.mesh.face_count && a.mesh.vertex_count==b.mesh.vertex_count);
    CHECK(!memcmp(a.mesh.faces,b.mesh.faces,a.mesh.face_count*sizeof(*a.mesh.faces)) && !memcmp(a.mesh.vertices,b.mesh.vertices,a.mesh.vertex_count*sizeof(*a.mesh.vertices)));
    CHECK(a.resident_bytes<=1024*1024 && a.peak_bytes<=1024*1024);
    printf("PASS actual Driller26/64 STAR decode, mixed cut/history, oldRFCT/RGCH; resident%u peak%u history%u template%u work%u\n",a.resident_bytes,a.peak_bytes,n,(unsigned)sizeof(rf_geomod_template),(unsigned)sizeof(rf_geomod_multi_work));
    free(wire);free(roundtrip);rf_geomod_terrain_close(&live);rf_geomod_terrain_close(&copy);return 0;
}
