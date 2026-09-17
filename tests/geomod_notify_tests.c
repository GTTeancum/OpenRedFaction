#include "rf/geomod_notify.h"
#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"notify line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int debris_cases(void)
{
 static const struct {uint32_t inputs[9];int32_t bounces;uint32_t output;} cases[]={
#include "fixtures/debris_postedit.inc"
 };
 uint32_t i;float in[9],age;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  memcpy(in,cases[i].inputs,sizeof(in));age=-99;
  CHECK(!rf_geomod_notify_debris(in,cases[i].bounces,in[7],in[8],in+3,in[6],&age));
  CHECK(!memcmp(&age,&cases[i].output,4));
 }
 age=123;in[0]=NAN;
 CHECK(rf_geomod_notify_debris(in,0,1,2,in+3,2,&age)==RF_FORMAT && age==123);
 in[0]=0;CHECK(rf_geomod_notify_debris(in,0,1,2,in+3,INFINITY,&age)==RF_FORMAT && age==123);
 CHECK(rf_geomod_notify_debris(NULL,0,1,2,in+3,2,&age)==RF_RANGE && age==123);
 puts("PASS168 original settled-debris cleanup cases and invalid-input preservation");return 0;
}
static int fragment_box_cases(void)
{
 static const struct {uint32_t inputs[9],count,output[6];} cases[]={
#include "fixtures/geomod_changed_box.inc"
 };
 rf_geomod_changed_box boxes[32],before[32],fragment;float values[9];uint32_t i,count;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  memset(boxes,0xa5,sizeof(boxes));memcpy(before,boxes,sizeof(boxes));memcpy(values,cases[i].inputs,36);memcpy(&fragment,values,24);count=cases[i].count;
  CHECK(!rf_geomod_notify_append_fragment_box(&fragment,values+6,boxes,&count));
  CHECK(count==(cases[i].count<32?cases[i].count+1:32));
  if(cases[i].count<32)memcpy(before+cases[i].count,cases[i].output,24);
  CHECK(!memcmp(before,boxes,sizeof(boxes)));
 }
 count=0;values[6]=NAN;memcpy(before,boxes,sizeof(boxes));
 CHECK(rf_geomod_notify_append_fragment_box(&fragment,values+6,boxes,&count)==RF_FORMAT && count==0 && !memcmp(boxes,before,sizeof(boxes)));
 puts("PASS27 original changed-fragment boxes and capacity/invalid-input preservation");return 0;
}
static int component_cases(void)
{
 static const struct {uint32_t words[102];int32_t selector;uint32_t result;} cases[]={
#include "fixtures/geomod_component_classify.inc"
 };
 rf_collision_face faces[6];rf_collision_bounds bounds;int32_t labels[6]={0};float values[102];uint32_t i,j,k,solid;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  memcpy(values,cases[i].words,sizeof(values));memcpy(&bounds,values,24);memset(faces,0,sizeof(faces));
  for(j=0;j<6;j++) {
   float *src=values+6+j*16;rf_collision_bounds b;
   memcpy(faces[j].plane,src,16);faces[j].vertices=(const float (*)[3])(src+4);faces[j].count=4;
   CHECK(!rf_collision_vertex_bounds(faces[j].vertices,4,&b));
   for(k=0;k<3;k++){faces[j].minimum[k]=b.minimum[k];faces[j].maximum[k]=b.maximum[k];}
  }
  solid=99;CHECK(!rf_geomod_component_classify(faces,labels,6,cases[i].selector,&bounds,&solid));CHECK(solid==cases[i].result);
 }
 solid=99;bounds.minimum[0]=NAN;
 CHECK(rf_geomod_component_classify(faces,labels,6,0,&bounds,&solid)==RF_FORMAT && solid==99);
 puts("PASS24 original component orientation classifications");return 0;
}
static int placement_cases(void)
{
 static const struct {uint32_t inputs[24],output[34];} cases[]={
#include "fixtures/geomod_piece_placement.inc"
 };
 float vertices[8][3],local[8][3],kept[8][3];rf_geomod_piece_placement placement,before;uint32_t i;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  memcpy(vertices,cases[i].inputs,sizeof(vertices));
  CHECK(!rf_geomod_piece_recenter(vertices,8,local,&placement));
  {uint32_t actual[34],j;memcpy(actual,&placement,40);memcpy(actual+10,local,96);
   for(j=0;j<34;j++)if(actual[j]!=cases[i].output[j])fprintf(stderr,"placement case%u word%u actual%u expected%u\n",i,j,actual[j],cases[i].output[j]);}
  CHECK(!memcmp(&placement,cases[i].output,40) && !memcmp(local,cases[i].output+10,96));
  CHECK(!rf_geomod_piece_recenter(vertices,8,vertices,&placement));
  CHECK(!memcmp(&placement,cases[i].output,40) && !memcmp(vertices,cases[i].output+10,96));
 }
 before=placement;memcpy(kept,local,sizeof(kept));vertices[7][2]=NAN;
 CHECK(rf_geomod_piece_recenter(vertices,8,local,&placement)==RF_FORMAT && !memcmp(&placement,&before,sizeof(before)) && !memcmp(local,kept,sizeof(kept)));
 puts("PASS12 original piece placement cases, in-place operation and failure atomicity");return 0;
}
static int connectivity_cases(void)
{
 static const struct {uint32_t faces,vertices[9],excluded,count,largest,labels[3];} cases[]={
#include "fixtures/geomod_components.inc"
 };
 rf_geomod_vertex vertices[9];rf_geomod_face faces[3];rf_collision_face_filter filters[3];
 rf_geomod_mesh_view mesh={vertices,faces,0,0,0};uint32_t work[128],labels[3],count,largest,words,i,j;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  memset(vertices,0,sizeof(vertices));memset(filters,0,sizeof(filters));mesh.face_count=cases[i].faces;mesh.vertex_count=mesh.face_count*3;
  for(j=0;j<mesh.vertex_count;j++){float v=(float)cases[i].vertices[j];vertices[j].position[0]=v;vertices[j].position[1]=v*v;vertices[j].position[2]=(j&1)?-0.0f:0.0f;vertices[j].uv[0]=(float)j;}
  for(j=0;j<mesh.face_count;j++){faces[j]=(rf_geomod_face){j*3,3,0,0};if(cases[i].excluded&(1u<<j))filters[j].face_flags=4;}
  CHECK(!rf_geomod_component_work_size(&mesh,&words) && words<=128);
  CHECK(!rf_geomod_mesh_components(&mesh,filters,work,128,labels,&count,&largest));
  CHECK(count==cases[i].count && largest==cases[i].largest && !memcmp(labels,cases[i].labels,mesh.face_count*4));
 }
 for(j=0;j<mesh.face_count;j++)filters[j].face_flags=4;
 CHECK(!rf_geomod_mesh_components(&mesh,filters,work,128,labels,&count,&largest) && count==0 && largest==UINT32_MAX);
 for(j=0;j<mesh.face_count;j++)CHECK(labels[j]==UINT32_MAX);
 labels[0]=123;count=456;largest=789;
 CHECK(rf_geomod_mesh_components(&mesh,filters,work,words-1,labels,&count,&largest)==RF_RANGE && labels[0]==123 && count==456 && largest==789);
 vertices[0].position[0]=NAN;
 CHECK(rf_geomod_mesh_components(&mesh,filters,work,128,labels,&count,&largest)==RF_FORMAT && labels[0]==123 && count==456 && largest==789);
 puts("PASS6 original-derived component graphs with distinct corner UV and failure preservation");return 0;
}
static int radial_cases(void)
{
 const uint32_t kinds[]={0,1,4,4,4,7},physics[]={0,1,0x10,0x80},flags[]={0x400000,0x400004,0x404000};
 const float scalar[]={4,4,2.999f,3,4,4},distance[]={2.999f,3,3.001f};
 rf_geomod_notify_change change={NULL,0,0,2};rf_geomod_notify_object object={0};rf_geomod_notify_radial radial={0};rf_geomod_notify_result result,kept;
 uint32_t k,p,f,c,g,d,count=0;radial.body_radius=1;
 for(k=0;k<6;k++)for(p=0;p<4;p++)for(f=0;f<3;f++)for(c=0;c<4;c++)for(g=0;g<3;g++)for(d=0;d<3;d++){
  uint32_t special,near,wake,retire;
  object.kind=kinds[k];object.physics_flags=physics[p];object.object_flags=flags[f];radial.scalar_78=scalar[k];
  radial.class_present=c!=0;radial.class_flags=c<2?0:c-1;radial.suppress_retirement=g==1;radial.network_active=g==2;radial.position[0]=distance[d];
  special=kinds[k]==1||kinds[k]==7||(kinds[k]==4&&scalar[k]<3);near=d==0;wake=near&&!special&&(physics[p]&0x71u);
  retire=near&&special&&!(flags[f]&(4u|0x4000u))&&!(radial.class_present&&((radial.class_flags&1u)||g));
  CHECK(!rf_geomod_notify_object_change(&object,&change,&radial,&result));
  CHECK(result.action==(wake?1u:retire?2u:0u)&&!result.overlaps);
  CHECK(result.object_flags==(flags[f]|(wake?0x06000000u:0)|(retire?2u:0))&&result.physics_flags==(physics[p]|(wake?0x80000000u:0)));count++;
 }
 /* Kind4 large prop: boxes retire while radius wakes. Neither action cancels. */
 {rf_geomod_changed_box box={{-1,-1,-1},{1,1,1}};
  change.boxes=&box;change.count=1;change.retirement_enabled=1;object=(rf_geomod_notify_object){4,0x400000,1,0,{{-.5f,-.5f,-.5f},{.5f,.5f,.5f}}};
  radial=(rf_geomod_notify_radial){{0,0,0},{0,0,0},1,3,0,0,0,0};CHECK(!rf_geomod_notify_object_change(&object,&change,&radial,&result));
  CHECK(result.action==3&&result.object_flags==0x06400002u&&result.physics_flags==0x80000001u&&result.overlaps==1);
 }
 change.boxes=NULL;change.count=0;memset(&result,0xa5,sizeof(result));kept=result;
 CHECK(rf_geomod_notify_object_change(&object,&change,NULL,&result)==RF_RANGE&&!memcmp(&result,&kept,sizeof(result)));
 radial.body_radius=-1;CHECK(rf_geomod_notify_object_change(&object,&change,&radial,&result)==RF_FORMAT&&!memcmp(&result,&kept,sizeof(result)));radial.body_radius=1;
 radial.position[1]=NAN;CHECK(rf_geomod_notify_object_change(&object,&change,&radial,&result)==RF_FORMAT&&!memcmp(&result,&kept,sizeof(result)));radial.position[1]=0;
 radial.class_present=2;CHECK(rf_geomod_notify_object_change(&object,&change,&radial,&result)==RF_RANGE&&!memcmp(&result,&kept,sizeof(result)));radial.class_present=0;
 object.parent_is_kind8=1;CHECK(!rf_geomod_notify_object_change(&object,&change,&radial,&result)&&!result.action);object.parent_is_kind8=0;
 object.object_flags=0;CHECK(!rf_geomod_notify_object_change(&object,&change,&radial,&result)&&!result.action);
 printf("PASS %u original-derived radius cases and combined/output rollback\n",count);return 0;
}
int main(void)
{
 const uint32_t kinds[]={0,1,4,7},physics[]={0,1,0x10,0x80},flags[]={0,0x400000,0x400004,0x480000};
 rf_geomod_changed_box boxes[32];rf_geomod_notify_change change={boxes,1,1,0};rf_geomod_notify_object object={0};rf_geomod_notify_result result,kept;
 uint32_t k,p,f,r,cases=0;boxes[0]=(rf_geomod_changed_box){{-1,-1,-1},{1,1,1}};
 for(k=0;k<4;k++)for(p=0;p<4;p++)for(f=0;f<4;f++)for(r=0;r<3;r++){
  float x=r==0?0:r==1?2:2.01f;uint32_t eligible,wake,retire;
  object=(rf_geomod_notify_object){kinds[k],flags[f],physics[p],0,{{x-1,-1,-1},{x+1,1,1}}};
  eligible=(flags[f]&0x400000u)&&!(flags[f]&0x80000u)&&r==0;wake=eligible&&kinds[k]!=4&&(physics[p]&0x71u);retire=eligible&&!wake&&!(flags[f]&4u);
  CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result));
  CHECK(result.action==(wake?RF_GEOMOD_NOTIFY_WAKE:retire?RF_GEOMOD_NOTIFY_RETIRE:RF_GEOMOD_NOTIFY_NONE));
  CHECK(result.physics_flags==(physics[p]|(wake?0x80000000u:0))&&result.object_flags==(flags[f]|(wake?0x06000000u:0)|(retire?2u:0))&&result.overlaps==eligible);cases++;
 }
 object=(rf_geomod_notify_object){0,0x400000,1,1,{{-1,-1,-1},{1,1,1}}};CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result)&&!result.action);object.parent_is_kind8=0;
 change.retirement_enabled=0;CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result)&&result.action==RF_GEOMOD_NOTIFY_WAKE);
 object.kind=4;change.retirement_enabled=256;CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result)&&!result.action);
 change.retirement_enabled=257;CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result)&&result.action==RF_GEOMOD_NOTIFY_RETIRE);
 for(k=1;k<32;k++)boxes[k]=boxes[0];change.count=32;CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result)&&result.overlaps==32&&result.object_flags==0x400002);
 change.count=0;change.boxes=NULL;CHECK(!rf_geomod_notify_changed_boxes(&object,&change,&result)&&!result.action);change.count=1;change.boxes=boxes;
 memset(&result,0xa5,sizeof(result));kept=result;
#define BAD(code) do{CHECK(rf_geomod_notify_changed_boxes(&object,&change,&result)==code&&!memcmp(&result,&kept,sizeof(result)));}while(0)
 change.radius=1;BAD(RF_NOT_FOUND);change.radius=NAN;BAD(RF_FORMAT);change.radius=0;
 change.count=33;BAD(RF_RANGE);change.count=1;object.parent_is_kind8=2;BAD(RF_RANGE);object.parent_is_kind8=0;
 boxes[0].minimum[0]=2;BAD(RF_FORMAT);boxes[0].minimum[0]=-1;object.bounds.maximum[1]=INFINITY;BAD(RF_FORMAT);object.bounds.maximum[1]=1;
 CHECK(rf_geomod_notify_changed_boxes(NULL,&change,&result)==RF_RANGE&&!memcmp(&result,&kept,sizeof(result)));
 CHECK(!radial_cases());
 CHECK(!debris_cases());
 CHECK(!fragment_box_cases());
 CHECK(!component_cases());
 CHECK(!placement_cases());
 CHECK(!connectivity_cases());
 printf("PASS %u original-derived changed-box rows plus parent/enable/multi-box/unsupported/rollback tests\n",cases);return 0;
}
