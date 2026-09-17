#include "rf/material.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static void word(unsigned char *p,uint32_t n){memcpy(p,&n,4);}
int main(void) {
    unsigned char data[160]={0},pixels[16]={255,0,0,0,0,255,0,255,0,0,255,0,255,255,255,255};
    uint32_t face_offset=0,offsets[2]={0,2},slots[2]={1,0},ids[2]={0,1000},texture=99,slot=99,color=99,i;
    float positions[3][3]={{0,0,1},{1,0,1},{0,1,1}},uv[3][2]={{.5f,0},{.75f,0},{.5f,.25f}};
    float scratch[3][3],coordinates[3][2],point[3]={.1f,.1f,1};
    rf_geometry geometry={0};rf_material items[2]={0};rf_geometry_materials materials={0};
    rf_geometry_texture_workspace work={scratch,coordinates,3};rf_geometry_material_collision base={0};
    rf_geometry_runtime_surface rows[2]={{1000,1,3,{0,0,1,-1},positions,uv}};
    rf_geometry_material_runtime runtime={&base,rows,1};rf_collision_indexed_texture_backend backend={0},saved;
    rf_collision_face face={0};int32_t bitmaps[2];
    geometry.data=data;geometry.bytes=sizeof(data);geometry.faces=1;geometry.vertices=3;geometry.textures=2;
    geometry.face_offsets=&face_offset;geometry.vertices_offset=100;
    {float one=1;memcpy(data+8,&one,4);}word(data+20,UINT32_MAX);word(data+52,3);
    for(i=0;i<3;i++) {
        float p[3]={positions[i][0],positions[i][1],0};float t[2]={positions[i][0],positions[i][1]};
        word(data+56+i*12,i);memcpy(data+60+i*12,t,8);memcpy(data+100+i*12,p,12);
    }
    for(i=0;i<2;i++)items[i].image=(rf_image){2,2,16,7,pixels};
    materials.textures.items=items;materials.textures.count=2;materials.offsets=offsets;materials.slots=slots;materials.count=1;
    base.materials=&materials;base.geometry=&geometry;base.source_indices=ids;base.face_count=2;base.work=&work;
    CHECK(!rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend));CHECK(bitmaps[0]==1 && bitmaps[1]==0);
    CHECK(!rf_geometry_material_runtime_lookup(&runtime,1000,&texture,&slot) && texture==1 && slot==0);
    CHECK(!backend.sample(backend.context,1,&face,0,point,&color) && color==0xff00ff00);
    CHECK(!backend.sample(backend.context,0,&face,1,point,&color) && color==0x000000ff);
    saved=backend;
    rows[1]=rows[0];runtime.count=2;
    CHECK(rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend)==RF_FORMAT && !memcmp(&saved,&backend,sizeof(saved)));runtime.count=1;
    rows[0].id=0;CHECK(rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend)==RF_FORMAT);rows[0].id=1000;
    rows[0].plane[2]=0;CHECK(rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend)==RF_FORMAT);rows[0].plane[2]=1;
    uv[0][0]=NAN;CHECK(rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend)==RF_FORMAT);uv[0][0]=.5f;
    rows[0].texture=2;CHECK(rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend)==RF_FORMAT);rows[0].texture=1;
    ids[1]=1001;CHECK(rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend)==RF_NOT_FOUND);ids[1]=1000;
    texture=slot=99;CHECK(rf_geometry_material_runtime_lookup(&runtime,1001,&texture,&slot)==RF_NOT_FOUND && texture==99 && slot==99);
    color=99;CHECK(backend.sample(backend.context,1,&face,1,point,&color)==RF_FORMAT && color==99);
    items[0].status=RF_NOT_FOUND;CHECK(backend.sample(backend.context,1,&face,0,point,&color)==RF_NOT_FOUND && color==99);items[0].status=0;
    point[0]=2;CHECK(backend.sample(backend.context,1,&face,0,point,&color)==RF_NOT_FOUND && color==99);point[0]=.1f;
    /* Runtime IDs relocate independently of authored UV/material identity. */
    rows[0].id=ids[1]=2000;CHECK(!rf_geometry_material_runtime_bind(&runtime,bitmaps,2,&backend));
    CHECK(!backend.sample(backend.context,1,&face,0,point,&color) && color==0xff00ff00);
    puts("PASS mixed compiled/runtime material lookup, authored UV alpha sampling, rejection and ID relocation");return 0;
}
