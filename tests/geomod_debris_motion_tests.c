#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d case%u: %s\n",__LINE__,i,#x);return 1;}}while(0)
static const struct {uint32_t flag,inputs[9],output[3];} cases[]={
#include "fixtures/debris_motion.inc"
};
static const struct {uint32_t inputs[3],output;} gravity_cases[]={
#include "fixtures/debris_gravity.inc"
};
static const struct {uint32_t inputs[11],hit,amount;} actor_cases[]={
#include "fixtures/debris_actor.inc"
};
static const struct {uint32_t damage,packet[19];} blood_cases[]={
#include "fixtures/debris_blood.inc"
};
int main(void)
{
    uint32_t i;float input[11],output[3],before[3]={11,12,13};
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        memcpy(input,cases[i].inputs,sizeof(cases[i].inputs));
        CHECK(!rf_geomod_debris_motion(input,input+3,input[6],cases[i].flag,input[7],input[8],output));
        CHECK(!memcmp(output,cases[i].output,12));
        CHECK(!rf_geomod_debris_motion(input,input+3,input[6],cases[i].flag,input[7],input[8],input));
        CHECK(!memcmp(input,cases[i].output,12));
    }
    for(i=0;i<sizeof(gravity_cases)/sizeof(gravity_cases[0]);i++) {
        memcpy(input,gravity_cases[i].inputs,12);
        CHECK(!rf_geomod_debris_gravity(input[0],input[1],input[2],output));
        CHECK(!memcmp(output,&gravity_cases[i].output,4));
    }
    for(i=0;i<sizeof(actor_cases)/sizeof(actor_cases[0]);i++) {
        uint32_t hit;memcpy(input,actor_cases[i].inputs,sizeof(input));
        CHECK(!rf_geomod_debris_actor_contact(input,input+3,input[6],input+7,input[10],&hit,output));
        CHECK(hit==actor_cases[i].hit && !memcmp(output,&actor_cases[i].amount,4));
    }
    {uint32_t hit=99;output[0]=11;
     CHECK(rf_geomod_debris_actor_contact(input,input+3,-1,input+7,1,&hit,output)==RF_RANGE && hit==99 && output[0]==11);}
    for(i=0;i<sizeof(blood_cases)/sizeof(blood_cases[0]);i++) {
        rf_particle_spawn packet;float position[3]={1,2,3},damage;
        memcpy(&damage,&blood_cases[i].damage,4);
        CHECK(!rf_particle_blood_prepare(position,damage,37,6,&packet));
        CHECK(sizeof(packet)==76 && !memcmp(&packet,blood_cases[i].packet,76));
        {rf_particle_spawn before=packet;
         CHECK(rf_particle_blood_prepare(position,-1,37,6,&packet)==RF_RANGE && !memcmp(&packet,&before,76));}
    }
    output[0]=11;
    CHECK(rf_geomod_debris_gravity(0,NAN,.1f,output)==RF_RANGE && output[0]==11);
    CHECK(rf_geomod_debris_gravity(0,1,-1,output)==RF_RANGE && output[0]==11);
    memcpy(output,before,12);input[0]=NAN;
    CHECK(rf_geomod_debris_motion(input,input+3,.1f,0,0,0,output)==RF_RANGE && !memcmp(output,before,12));
    input[0]=0;CHECK(rf_geomod_debris_motion(input,input+3,.1f,1,NAN,0,output)==RF_RANGE && !memcmp(output,before,12));
    CHECK(rf_geomod_debris_motion(input,input+3,-1,0,0,0,output)==RF_RANGE && !memcmp(output,before,12));
    puts("PASS72 motion vectors and140 gravity results plus9 actor contacts and4 blood packets bit-exact against original, aliasing and atomic guards");return 0;
}
