#include "rf/geomod_publication_digest.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int rejected(rf_geomod_publication_digest_input *v)
{unsigned char out[32],old[32];memset(out,0xa5,32);memcpy(old,out,32);return rf_geomod_publication_digest(v,out)!=RF_OK&&!memcmp(out,old,32);}
int main(void)
{
    /* Independent Python hashlib canonical 328-byte RFAP vector. */
    static const unsigned char expected[32]={0x3b,0xce,0x9a,0x7d,0x5a,0x92,0xc5,0x8b,0xad,0xc6,0x10,0xfc,0x5e,0xe5,0xb2,0xca,0x45,0x81,0x02,0x3a,0x7b,0x56,0x0d,0xff,0xd6,0xd5,0xe0,0x4d,0x37,0x69,0x55,0xf0};
    rf_geomod_vertex vertices[6]={{{0,0,0},{0,0}},{{1,0,0},{1,0}},{{0,1,0},{0,1}},
        {{0,0,1},{0,0}},{{1,0,1},{1,0}},{{0,1,1},{0,1}}},copy[6];
    rf_geomod_face faces[2]={{0,3,7,550},{3,3,8,UINT32_MAX}};
    rf_geomod_publication_origin origins[2]={{0,94,550,149},{1,94,UINT32_MAX,149}};
    rf_geomod_digest_material materials[2]={0};rf_geomod_digest_chart charts[3]={0};uint32_t keys[2]={11,22},i;
    unsigned char pixels[2][4]={{1,2,3,4},{5,6,7,8}},chart_pixels[32],digest[32],again[32];
    rf_geomod_publication_digest_input in={0};
    for(i=0;i<2;i++) {
        materials[i].key=7+i;strcpy(materials[i].image.name,i?"rock.tga":"wood.tga");
        materials[i].image.width=materials[i].image.height=1;materials[i].image.format=4;
        materials[i].image.bytes_per_pixel=materials[i].image.bytes=4;materials[i].image.pixels=pixels[i];
        charts[i].key=keys[i];charts[i].owner=94;charts[i].source_face=i?UINT32_MAX:550;
        charts[i].retained_map=i?0:UINT32_MAX;
    }
    charts[1].kind=RF_GEOMOD_DIGEST_GENERATED_CHART;charts[1].image=materials[1].image;
    for(i=0;i<16;i++){chart_pixels[i*2]=0x21;chart_pixels[i*2+1]=0x84;}
    charts[1].image.width=charts[1].image.height=4;charts[1].image.format=5;
    charts[1].image.bytes_per_pixel=2;charts[1].image.bytes=32;charts[1].image.pixels=chart_pixels;
    strcpy(charts[1].image.name,"cut/chart/0");charts[1].projection.axes[1]=1;
    charts[1].projection.scale[0]=charts[1].projection.scale[1]=1;
    in.mesh=(rf_geomod_mesh_view){vertices,faces,6,2,0};in.origins=origins;in.face_charts=keys;
    in.materials=materials;in.material_count=2;in.charts=charts;in.chart_count=2;in.publication_policy=in.material_policy=1;
    CHECK(!rf_geomod_publication_digest(&in,digest));
    CHECK(!memcmp(digest,expected,32));
    printf("CANONICAL ");for(i=0;i<32;i++)printf("%02x",digest[i]);puts("");
    for(i=0;i<2;i++) {
        CHECK(!rf_geomod_image_content_digest(&materials[i].image,materials[i].content_digest));
        materials[i].prehashed=1;materials[i].image.pixels=NULL;
    }
    CHECK(!rf_geomod_image_content_digest(&charts[1].image,charts[1].content_digest));
    charts[1].prehashed=1;charts[1].image.pixels=NULL;
    CHECK(!rf_geomod_publication_digest(&in,again)&&!memcmp(digest,again,32));
    charts[1].image.pixels=chart_pixels;CHECK(rejected(&in));charts[1].image.pixels=NULL;
    materials[0].prehashed=2;CHECK(rejected(&in));materials[0].prehashed=1;
    for(i=0;i<2;i++){materials[i].prehashed=0;materials[i].image.pixels=pixels[i];}
    charts[1].prehashed=0;charts[1].image.pixels=chart_pixels;
    /* Runtime lookup key/table relocation and scene source-face rewriting. */
    materials[0].key=faces[0].material=999;charts[0].key=keys[0]=777;
    origins[0].reference=123;faces[0].source_face=123;in.source_domain=RF_GEOMOD_DIGEST_COMPILED_SOURCE;
    memcpy(copy,vertices,sizeof(copy));in.mesh.vertices=copy;in.mesh.generation=999;
    CHECK(!rf_geomod_publication_digest(&in,again)&&!memcmp(digest,again,32));
    materials[0].key=faces[0].material=7;charts[0].key=keys[0]=11;
    origins[0].reference=149;faces[0].source_face=550;in.source_domain=0;in.mesh.vertices=vertices;
    {rf_geomod_digest_material m=materials[0];rf_geomod_digest_chart c=charts[0];materials[0]=materials[1];materials[1]=m;charts[0]=charts[1];charts[1]=c;
     CHECK(!rf_geomod_publication_digest(&in,again)&&!memcmp(digest,again,32));
     m=materials[0];c=charts[0];materials[0]=materials[1];materials[1]=m;charts[0]=charts[1];charts[1]=c;}
    /* Face order remains meaningful, unlike lookup-table order. */
    {rf_geomod_face f=faces[0];rf_geomod_publication_origin o=origins[0];uint32_t key=keys[0];
     faces[0]=faces[1];faces[1]=f;faces[0].first=0;faces[1].first=3;origins[0]=origins[1];origins[1]=o;
     keys[0]=keys[1];keys[1]=key;memcpy(copy,vertices+3,3*sizeof(*copy));memcpy(copy+3,vertices,3*sizeof(*copy));in.mesh.vertices=copy;
     CHECK(!rf_geomod_publication_digest(&in,again)&&memcmp(digest,again,32));
     f=faces[0];o=origins[0];key=keys[0];faces[0]=faces[1];faces[1]=f;faces[0].first=0;faces[1].first=3;
     origins[0]=origins[1];origins[1]=o;keys[0]=keys[1];keys[1]=key;in.mesh.vertices=vertices;}
    vertices[0].uv[0]=.25f;CHECK(!rf_geomod_publication_digest(&in,again)&&memcmp(digest,again,32));vertices[0].uv[0]=0;
    pixels[0][0]^=1;CHECK(!rf_geomod_publication_digest(&in,again)&&memcmp(digest,again,32));pixels[0][0]^=1;
    charts[1].retained_map=1;CHECK(!rf_geomod_publication_digest(&in,again)&&memcmp(digest,again,32));charts[1].retained_map=0;
    charts[1].projection.offset[0]=.25f;CHECK(!rf_geomod_publication_digest(&in,again)&&memcmp(digest,again,32));charts[1].projection.offset[0]=0;
    keys[1]=99;CHECK(rejected(&in));keys[1]=22;
    materials[1].key=7;CHECK(rejected(&in));materials[1].key=8;
    charts[1].key=11;CHECK(rejected(&in));charts[1].key=22;
    charts[2]=charts[1];charts[2].key=33;in.chart_count=3;CHECK(rejected(&in));in.chart_count=2;
    charts[1].image.width=512;CHECK(rejected(&in));charts[1].image.width=4;
    charts[1].image.format=7;CHECK(rejected(&in));charts[1].image.format=5;
    charts[1].retained_map=UINT32_MAX;CHECK(rejected(&in));charts[1].retained_map=0;
    charts[1].kind=RF_GEOMOD_DIGEST_UNLIT;CHECK(rejected(&in));charts[1].kind=RF_GEOMOD_DIGEST_GENERATED_CHART;
    charts[0].owner=71;CHECK(rejected(&in));charts[0].owner=94;
    faces[0].source_face=149;CHECK(rejected(&in));faces[0].source_face=550;
    faces[1].first=2;CHECK(rejected(&in));faces[1].first=3;
    in.mesh.vertex_count=7;CHECK(rejected(&in));in.mesh.vertex_count=6;
    vertices[0].position[0]=NAN;CHECK(rejected(&in));vertices[0].position[0]=0;
    in.material_policy=2;CHECK(rejected(&in));in.material_policy=1;
    CHECK(!rf_geomod_publication_digest(&in,again)&&!memcmp(digest,again,32));
    {rf_geomod_publication_digest_input empty={0};empty.publication_policy=empty.material_policy=1;
     CHECK(!rf_geomod_publication_digest(&empty,again)&&memcmp(digest,again,32));
     empty.mesh.vertex_count=1;CHECK(rejected(&empty));empty.mesh.vertex_count=0;
     empty.chart_count=1;CHECK(rejected(&empty));}

    {
        uint32_t count=RF_GEOMOD_PUBLICATION_FACES,n;rf_geomod_publication_digest_input large=in;
        rf_geomod_vertex *v=malloc(count*3*sizeof(*v));rf_geomod_face *f=malloc(count*sizeof(*f));
        rf_geomod_publication_origin *o=malloc(count*sizeof(*o));uint32_t *k=malloc(count*sizeof(*k));
        CHECK(v && f && o && k);
        for(n=0;n<count;n++){memcpy(v+n*3,vertices+(n%2)*3,3*sizeof(*v));f[n]=faces[n%2];f[n].first=n*3;o[n]=origins[n%2];k[n]=keys[n%2];}
        large.mesh=(rf_geomod_mesh_view){v,f,count*3,count,0};large.origins=o;large.face_charts=k;
        CHECK(!rf_geomod_publication_digest(&large,digest));
        v[count*3-1].uv[0]+=.125f;CHECK(!rf_geomod_publication_digest(&large,again) && memcmp(digest,again,32));
        k[count-1]=UINT32_MAX;CHECK(rejected(&large));k[count-1]=keys[(count-1)%2];
        large.mesh.face_count=count+1;CHECK(rejected(&large));large.mesh.face_count=count;
        large.mesh.vertex_count=RF_GEOMOD_PUBLICATION_VERTICES+1;CHECK(rejected(&large));
        printf("CAPACITY publication faces%u vertices%u tail mutation/rejection verified\n",count,count*3);
        free(k);free(o);free(f);free(v);
    }
    puts("PASS publication digest stable lookup identity, ordered output, content/provenance gates, atomic failures");return 0;
}
