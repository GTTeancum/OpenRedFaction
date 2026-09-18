/* Actual APC mortar model resource and world preview submission. */
#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_apc_secondary_visual.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"APC mortar line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 static scene_stream stream;rf_preview_mesh mesh={0};rf_materials materials={0};
 rf_vpp meshes={0},maps[4]={{0}};scene_apc_secondary_visual *owner=NULL;
 float position[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,base,textures,submissions,vertices;char path[128];
 CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
 for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
 CHECK(!scene_apc_secondary_visual_open(&meshes,maps,4,512*1024,&owner));
 CHECK(!scene_apc_secondary_visual_basis((const float[3]){0,30,0},basis));CHECK(basis[7]==1);
 CHECK(!scene_apc_secondary_visual_basis((const float[3]){30,0,0},basis));CHECK(basis[6]==1);
 CHECK(scene_apc_secondary_visual_basis((const float[3]){0,0,0},basis)==RF_NOT_FOUND);
 {float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};uint32_t part,v,axis;
  for(part=0;part<owner->render.part_count;part++) {
   const rf_model_geometry *g=&owner->render.lods[owner->render.parts[part].first_lod].geometry;
   for(v=0;v<g->vertex_count;v++)for(axis=0;axis<3;axis++) {
    float x=g->vertices[v].position[axis];if(x<lo[axis])lo[axis]=x;if(x>hi[axis])hi[axis]=x;
   }
  }
  printf("APC_MORTAR_AABB min=%g,%g,%g max=%g,%g,%g bound=%g,%g,%g,%g\n",lo[0],lo[1],lo[2],hi[0],hi[1],hi[2],owner->render.bound[0],owner->render.bound[1],owner->render.bound[2],owner->render.bound[3]);
 }
 CHECK(owner->resident_bytes<=512*1024 && owner->peak_bytes<=512*1024);
 printf("APC_MORTAR_MEMORY resident=%u peak=%u\n",owner->resident_bytes,owner->peak_bytes);
 rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
 CHECK(!scene_apc_secondary_visual_merge(owner,&materials,&base,&textures));
 CHECK(textures && materials.count==textures && !owner->materials.textures.items);
 stream.mesh=&mesh;stream.materials=&materials;stream.capacity=32768*sizeof(rf_preview_vertex);
 mesh.vertices=calloc(1,stream.capacity);CHECK(mesh.vertices);
 stream.npc_memory=malloc(4096*96);stream.npc_indices=malloc(24576*sizeof(uint16_t));stream.npc_pool=calloc(1,sizeof(*stream.npc_pool));
 CHECK(stream.npc_memory && stream.npc_indices && stream.npc_pool);model_backend=NULL;
 stream.npc_view.rotation[0]=stream.npc_view.rotation[4]=stream.npc_view.rotation[8]=1;
 memcpy(stream.npc_view.camera,owner->render.bound,12);stream.npc_view.camera[2]-=3*owner->render.bound[3];
 stream.npc_view.screen[0]=320;stream.npc_view.screen[1]=-320;stream.npc_view.screen[2]=320;stream.npc_view.screen[3]=240;
 stream.npc_view.perspective=stream.npc_view.compute_clip=stream.npc_view.clipping=1;
 CHECK(!scene_apc_secondary_visual_draw(owner,&stream,position,(const float[3]){0,0,30},base,textures,&submissions,&vertices));
 CHECK(submissions && vertices && mesh.count==vertices);
 for(i=0;i<mesh.count;i++)CHECK(mesh.vertices[i].material>=base && mesh.vertices[i].material<base+textures &&
   isfinite(mesh.vertices[i].position[0]) && isfinite(mesh.vertices[i].position[1]) && isfinite(mesh.vertices[i].position[2]));
 printf("APC_MORTAR_DRAW submissions=%u vertices=%u textures=%u\n",submissions,vertices,textures);
 scene_apc_secondary_visual_close(&owner);rf_materials_close(&materials);free(mesh.vertices);
 free(stream.npc_memory);free(stream.npc_indices);free(stream.npc_pool);
 rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);return 0;
}
