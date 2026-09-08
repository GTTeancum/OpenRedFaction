#include "rf/motion.h"
#include <math.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(void)
{
    rf_motion_position_key keys[2] = {{0,{0,0,0},{0,0,0},{0,0,0}}, {100,{4,8,12},{4,8,12},{4,8,12}}};
    float out[3]={99,99,99}, sentinel[3]={99,99,99};
    unsigned char packed[8] = {0,0, 0,64, 0,192, 0,128};
    float rotation[4] = {99,99,99,99};
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
