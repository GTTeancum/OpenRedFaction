#include "rf/geomod_notify.h"
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
 printf("PASS %u original-derived changed-box rows plus parent/enable/multi-box/unsupported/rollback tests\n",cases);return 0;
}
