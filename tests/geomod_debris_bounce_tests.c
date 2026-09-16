#include "rf/geomod.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}} while(0)
/* Captured original RF.exe48fac9..48fbe7, SHA256 b8fb9ab4c9bf...
 * artifacts/future-vehicles-re/debris_impulse_tail.py. No formula-generated expected results. */
typedef struct fixture {uint32_t input[8],seed,output[8],next;} fixture;
static const fixture cases[]={
    {{0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0x3a5af4a0u,0xbac980f2u,0x3a960b88u,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 0 */
    {{0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3c888889u,0x411ccccdu},1u,{0x3a6df320u,0x3ab234dfu,0xba04a356u,0xbf2d5693u,0xbeccffaeu,0x3f1e1000u,0x4094c587u,0x3dcd919au},1030492215u}, /* oracle 1 */
    {{0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3e000000u,0x411ccccdu},7654321u,{0x3b5f0d48u,0x3a267c17u,0x3a367d15u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 7 */
    {{0xbc23d70au,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0x3a5af4a0u,0x403fe6d0u,0x3a960b88u,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 8 */
    {{0xbdcccccdu,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3c888889u,0x411ccccdu},1u,{0x3c14b7f0u,0x3c5ec217u,0xbba5cc2bu,0xbf2d5693u,0xbeccffaeu,0x3f1e1000u,0x4094c587u,0x3dcd919au},1030492215u}, /* oracle 17 */
    {{0xbdcccccdu,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0x3c08d8f0u,0x403f041fu,0x3c3b8e6bu,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 24 */
    {{0xbf800000u,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0x3dab0f20u,0x40362934u,0x3dea7205u,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 40 */
    {{0xc1000000u,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x00000000u,0x3e000000u,0x411ccccdu},7654321u,{0x402e4260u,0x4060843du,0x3f0e91b9u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 63 */
    {{0x00000000u,0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0xbc93b996u,0x3e248a2bu,0x3cc6634eu,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x4182aaacu},773150046u}, /* oracle 64 */
    {{0x00000000u,0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x3c888889u,0x411ccccdu},1u,{0x3c029574u,0x3e25b43du,0xbcaf7252u,0xbf2d5693u,0xbeccffaeu,0x3f1e1000u,0x4094c587u,0x4182aaabu},1030492215u}, /* oracle 65 */
    {{0x00000000u,0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x3e000000u,0x411ccccdu},4294967295u,{0x3d755328u,0x3f9c81fcu,0xbd395251u,0x3f691891u,0xbe8607ecu,0xbea3d800u,0x40795a06u,0x42f50000u},515807877u}, /* oracle 70 */
    {{0x00000000u,0xbc23d70au,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x3e000000u,0x411ccccdu},7654321u,{0xbd830a3du,0x3f9c6929u,0x403c439eu,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x42f50000u},965384295u}, /* oracle 79 */
    {{0x00000000u,0xbdcccccdu,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x3e000000u,0x411ccccdu},7654321u,{0xbd8c96e7u,0x3f9c61e6u,0xbd804294u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x41440000u},965384295u}, /* oracle 87 */
    {{0x00000000u,0xbf800000u,0x00000000u,0x00000000u,0x3f800000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0xbdf7dd88u,0x3e150a60u,0x3e266f9cu,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3e2740dbu},773150046u}, /* oracle 96 */
    {{0x00000000u,0xbf800000u,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x3c888889u,0x411ccccdu},0u,{0xbdf7dd88u,0x3e150a60u,0x404a66fau,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3e2740dbu},773150046u}, /* oracle 104 */
    {{0x00000000u,0xc1000000u,0x40400000u,0x00000000u,0x3f800000u,0x00000000u,0x3e000000u,0x411ccccdu},7654321u,{0xbf0e91b9u,0x402e4260u,0x401f7bc3u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 127 */
    {{0x00000000u,0x00000000u,0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x3c888889u,0x411ccccdu},0u,{0xba960b88u,0xbac980f2u,0x3a5af4a0u,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 128 */
    {{0x00000000u,0x00000000u,0xbc23d70au,0x00000000u,0x00000000u,0x3f800000u,0x3e000000u,0x411ccccdu},7654321u,{0xba367d15u,0x3a267c17u,0x3b5f0d48u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 135 */
    {{0x00000000u,0x00000000u,0xbdcccccdu,0x00000000u,0x00000000u,0x3f800000u,0x3e000000u,0x411ccccdu},4294967295u,{0x3baedb77u,0x3b8416e6u,0x3c210838u,0x3f691891u,0xbe8607ecu,0xbea3d800u,0x40795a06u,0x3dcd74cdu},515807877u}, /* oracle 150 */
    {{0x40400000u,0x00000000u,0xbdcccccdu,0x00000000u,0x00000000u,0x3f800000u,0x3e000000u,0x411ccccdu},7654321u,{0x403f8df2u,0x3bd01b1eu,0x3d0b684au,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 159 */
    {{0x40400000u,0x00000000u,0xbf800000u,0x00000000u,0x00000000u,0x3f800000u,0x3c888889u,0x411ccccdu},0u,{0x4038ac70u,0xbe1d6cbdu,0x3dab0f20u,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 168 */
    {{0x40400000u,0x00000000u,0xbf800000u,0x00000000u,0x00000000u,0x3f800000u,0x3e000000u,0x411ccccdu},7654321u,{0x403b8b72u,0x3d8210f3u,0x3eae4260u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 175 */
    {{0x40400000u,0x00000000u,0xc1000000u,0x00000000u,0x00000000u,0x3f800000u,0x3c888889u,0x411ccccdu},0u,{0x4005637fu,0xbf9d6cbdu,0x3f2b0f20u,0xbd88416eu,0x3f054064u,0xbf59ec00u,0x4088b945u,0x3dcd8333u},773150046u}, /* oracle 184 */
    {{0x40400000u,0x00000000u,0xc1000000u,0x00000000u,0x00000000u,0x3f800000u,0x3e000000u,0x411ccccdu},7654321u,{0x401c5b92u,0x3f0210f3u,0x402e4260u,0xbf5ad468u,0xbde4e1ecu,0x3f01bc00u,0x4091b8d8u,0x3eaff400u},965384295u}, /* oracle 191 */
};
int main(void)
{
    rf_geomod_debris_bounce out,before;rf_random_state rng;float input[8],zero[3]={0},normal[3]={0,1,0},velocity[3]={1,-1,2};uint32_t i,j;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        memcpy(input,cases[i].input,sizeof(input));rng.value=cases[i].seed;
        CHECK(rf_geomod_debris_contact(input,input+3,input[6],input[7],&rng,&out)==RF_OK);
        CHECK(rng.value==cases[i].next);
        if(memcmp(&out,cases[i].output,sizeof(out))) {fprintf(stderr,"original fixture %u mismatch\n",i);return 1;}
    }
    memset(&before,0xa5,sizeof(before));
    for(i=0;i<11;i++) {
        float v[3]={1,-1,2},n[3]={0,1,0},dt=1.f/60,gravity=9.8f;
        if(i==0)dt=0;if(i==1)dt=-1;if(i==2)dt=NAN;if(i==3)gravity=-1;if(i==4)gravity=INFINITY;
        if(i==5)v[0]=NAN;if(i==6)n[1]=INFINITY;if(i==7)n[1]=0;if(i==8)n[1]=2;
        if(i==9){v[0]=FLT_MAX;v[1]=FLT_MAX;v[2]=FLT_MAX;n[0]=n[1]=n[2]=.577350269f;}
        if(i==10){v[0]=-FLT_MAX;n[0]=1;n[1]=0;}
        out=before;rng.value=123;CHECK(rf_geomod_debris_contact(v,n,dt,gravity,&rng,&out)!=RF_OK);
        CHECK(rng.value==123 && !memcmp(&out,&before,sizeof(out)));
    }
    out=before;rng.value=123;CHECK(rf_geomod_debris_contact(NULL,normal,1,9.8f,&rng,&out)==RF_RANGE);
    CHECK(rng.value==123 && !memcmp(&out,&before,sizeof(out)));
    CHECK(rf_geomod_debris_contact(velocity,NULL,1,9.8f,&rng,&out)==RF_RANGE);
    CHECK(rf_geomod_debris_contact(velocity,normal,1,9.8f,NULL,&out)==RF_RANGE);
    CHECK(rf_geomod_debris_contact(velocity,normal,1,9.8f,&rng,NULL)==RF_RANGE);
    /* A continuing zero-speed contact consumes its original draws but adds zero impulse. */
    rng.value=0;CHECK(rf_geomod_debris_contact(zero,normal,1.f/60,0,&rng,&out)==RF_OK);
    CHECK(rng.value==773150046u);for(j=0;j<3;j++)CHECK(out.velocity[j]==0);
    puts("PASS24 original debris bounce vectors, rollback guards and zero-speed contact");return 0;
}
