#include "rf/motion.h"
#include <math.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(void)
{
    int32_t ticks=99;
    CHECK(rf_motion_elapsed_ticks(NAN,&ticks)==RF_RANGE && ticks==99);
    CHECK(rf_motion_elapsed_ticks(INFINITY,&ticks)==RF_RANGE && ticks==99);
    CHECK(rf_motion_elapsed_ticks(1000000,&ticks)==RF_RANGE && ticks==99);
    CHECK(rf_motion_elapsed_ticks(0.2f,&ticks)==RF_OK && ticks==960);
    CHECK(rf_motion_elapsed_ticks(-0.2f,&ticks)==RF_OK && ticks==-960);
    rf_motion_weight_envelope envelope = {1,0,100,20,20};
    float weight=99;
    CHECK(rf_motion_sample_weight(NULL,0,0,&weight)==RF_RANGE && weight==99);
    envelope.fade_in=-1;
    CHECK(rf_motion_sample_weight(&envelope,0,0,&weight)==RF_FORMAT && weight==99);
    envelope.fade_in=20; envelope.weight=NAN;
    CHECK(rf_motion_sample_weight(&envelope,0,0,&weight)==RF_FORMAT && weight==99);
    envelope.weight=1;
    CHECK(rf_motion_sample_weight(&envelope,10,0,&weight)==RF_OK && weight==0.5f);
    CHECK(rf_motion_sample_weight(&envelope,90,0,&weight)==RF_OK && weight==0.5f);
    CHECK(rf_motion_sample_weight(&envelope,-10,1,&weight)==RF_OK && weight==1);
    envelope.weight=-1;
    CHECK(rf_motion_sample_weight(&envelope,50,1,&weight)==RF_OK && weight==0);
    rf_motion_position_key keys[2] = {{0,{0,0,0},{0,0,0},{0,0,0}}, {100,{4,8,12},{4,8,12},{4,8,12}}};
    float out[3]={99,99,99}, sentinel[3]={99,99,99};
    unsigned char packed[8] = {0,0, 0,64, 0,192, 0,128};
    float rotation[4] = {99,99,99,99};
    int16_t qa[4] = {0,0,0,16383}, qb[4] = {0,0,0,16383};
    int16_t qr[4] = {99,99,99,99}, qs[4] = {99,99,99,99};
    rf_motion_rotation_key rk[2] = {{0,{0,0,0,16383},0,0,{0,0}}, {100,{0,10000,0,12000},0,0,{0,0}}};
    CHECK(rf_motion_sample_rotation(NULL,0,0,rotation)==RF_OK && rotation[3]==1);
    CHECK(rf_motion_sample_rotation(rk,1,-1,rotation)==RF_OK);
    CHECK(rotation[0]==0 && rotation[3]>0.99f);
    CHECK(rf_motion_sample_rotation(rk,1,0,rotation)==RF_OK);
    CHECK(rf_motion_sample_rotation(rk,2,50,rotation)==RF_OK);
    rk[1].tick=0;
    CHECK(rf_motion_sample_rotation(rk,2,0,rotation)==RF_FORMAT);
    rk[1].tick=100; rk[1].incoming=-1;
    CHECK(rf_motion_sample_rotation(rk,2,50,rotation)==RF_FORMAT);
    rotation[0]=rotation[1]=rotation[2]=rotation[3]=99;
    CHECK(rf_motion_interpolate_rotation(NULL,qb,0,qr)==RF_RANGE);
    CHECK(rf_motion_interpolate_rotation(qa,qb,NAN,qr)==RF_RANGE);
    CHECK(rf_motion_interpolate_rotation(qa,qb,-1,qr)==RF_RANGE);
    CHECK(rf_motion_interpolate_rotation(qa,qb,2,qr)==RF_RANGE);
    CHECK(memcmp(qr,qs,sizeof(qr))==0);
    CHECK(rf_motion_interpolate_rotation(qa,qb,0.5f,qr)==RF_OK);
    CHECK(rf_motion_interpolate_rotation(qa,qb,0.5f,qa)==RF_OK);
    CHECK(memcmp(qa,qr,sizeof(qa))==0);
    CHECK(rf_motion_decode_rotation(packed,7,rotation)==RF_RANGE);
    CHECK(rotation[0]==99 && rotation[3]==99);
    CHECK(rf_motion_decode_rotation(NULL,8,rotation)==RF_RANGE);
    CHECK(rf_motion_decode_rotation(packed,8,NULL)==RF_RANGE);
    CHECK(rf_motion_decode_rotation(packed,8,rotation)==RF_OK);
    CHECK(rotation[0]==0 && rotation[1]>1 && rotation[2]<-1 && rotation[3]<-2);
    CHECK(rf_motion_sample_position(NULL,1,0,out)==RF_RANGE);
    keys[1].tick=0;
    CHECK(rf_motion_sample_position(keys,2,0,out)==RF_FORMAT);
    keys[1].tick=100; keys[0].outgoing[0]=NAN;
    CHECK(rf_motion_sample_position(keys,2,0,out)==RF_FORMAT);
    CHECK(memcmp(out,sentinel,sizeof(out))==0);
    keys[0].outgoing[0]=0;
    CHECK(rf_motion_sample_position(keys,2,50,out)==RF_OK);
    CHECK(out[0]==2 && out[1]==4 && out[2]==6);
    CHECK(rf_motion_sample_position(keys,2,-1,out)==RF_OK && out[0]==0);
    CHECK(rf_motion_sample_position(keys,2,101,out)==RF_OK && out[0]==4);
    CHECK(rf_motion_sample_position(NULL,0,0,out)==RF_OK && out[0]==0 && out[1]==0 && out[2]==0);
    return 0;
}
