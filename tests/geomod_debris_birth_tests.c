#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}} while(0)
/* Original prepared48ffa0..4900e4, tools/future_re/debris_birth.py. */
typedef struct fixture {uint32_t seed,radius,birth[11],state8,lifetime,state33,velocity[3],state35;} fixture;
static const fixture cases[]={
 {0u,0x3f000000u,{0x3a82ef35u,0x3c33f62bu,0xbe25897cu,0x3d4d2311u,0x39d7ab22u,0x00000005u,0xbd026a9cu,0x3f75a08eu,0xbe8f5800u,0x40c79482u,0x00000002u},2115878600u,0x3ffa7600u,2202588003u,{0x40ef3882u,0xbddb8d50u,0xc1162e69u},3255514357u},
 {0u,0x3f800000u,{0x3b02ef35u,0x3cb3f62bu,0xbea5897cu,0x3d4d2311u,0x39d7ab22u,0x00000005u,0xbd026a9cu,0x3f75a08eu,0xbe8f5800u,0x40c79482u,0x00000002u},2115878600u,0x3ffa7600u,2202588003u,{0x40ef3882u,0xbddb8d50u,0xc1162e69u},3255514357u},
 {0u,0x40000000u,{0x3b82ef35u,0x3d33f62bu,0xbf25897cu,0x3d4d2311u,0x39d7ab22u,0x00000005u,0xbd026a9cu,0x3f75a08eu,0xbe8f5800u,0x40c79482u,0x00000002u},2115878600u,0x3ffa7600u,2202588003u,{0x40ef3882u,0xbddb8d50u,0xc1162e69u},3255514357u},
 {0u,0x40800000u,{0x3c02ef35u,0x3db3f62bu,0xbfa5897cu,0x3d4d2311u,0x39d7ab22u,0x00000005u,0xbd026a9cu,0x3f75a08eu,0xbe8f5800u,0x40c79482u,0x00000002u},2115878600u,0x3ffa7600u,2202588003u,{0x40ef3882u,0xbddb8d50u,0xc1162e69u},3255514357u},
 {1u,0x3f000000u,{0xbb4e4936u,0xbaae255cu,0xbd457132u,0x3e1f85b4u,0x3f076721u,0x00000005u,0xbf16b73au,0x3f4eacb8u,0xbd250000u,0x40be99a6u,0x00000000u},1924036713u,0x405dfb00u,3115572192u,{0xc039a70fu,0x3f72e398u,0xc02131aeu},122505562u},
 {1u,0x3f800000u,{0xbbce4936u,0xbb2e255cu,0xbdc57132u,0x3e1f85b4u,0x3f076721u,0x00000005u,0xbf16b73au,0x3f4eacb8u,0xbd250000u,0x40be99a6u,0x00000000u},1924036713u,0x405dfb00u,3115572192u,{0xc039a70fu,0x3f72e398u,0xc02131aeu},122505562u},
 {1u,0x40000000u,{0xbc4e4936u,0xbbae255cu,0xbe457132u,0x3e1f85b4u,0x3f076721u,0x00000005u,0xbf16b73au,0x3f4eacb8u,0xbd250000u,0x40be99a6u,0x00000000u},1924036713u,0x405dfb00u,3115572192u,{0xc039a70fu,0x3f72e398u,0xc02131aeu},122505562u},
 {1u,0x40800000u,{0xbcce4936u,0xbc2e255cu,0xbec57132u,0x3e1f85b4u,0x3f076721u,0x00000005u,0xbf16b73au,0x3f4eacb8u,0xbd250000u,0x40be99a6u,0x00000000u},1924036713u,0x405dfb00u,3115572192u,{0xc039a70fu,0x3f72e398u,0xc02131aeu},122505562u},
 {4294967295u,0x3f000000u,{0x3ab846c6u,0xba71e33bu,0xbcd26caeu,0x3d6cff2eu,0x3d20fbe6u,0x00000003u,0x3ee9acefu,0x3f38e622u,0xbf050800u,0x40581074u,0x00000002u},2307720487u,0x405c7b00u,1289603814u,{0xc09925b4u,0xbf9e8167u,0xc12ef380u},2093555856u},
 {4294967295u,0x3f800000u,{0x3b3846c6u,0xbaf1e33bu,0xbd526caeu,0x3d6cff2eu,0x3d20fbe6u,0x00000003u,0x3ee9acefu,0x3f38e622u,0xbf050800u,0x40581074u,0x00000002u},2307720487u,0x405c7b00u,1289603814u,{0xc09925b4u,0xbf9e8167u,0xc12ef380u},2093555856u},
 {4294967295u,0x40000000u,{0x3bb846c6u,0xbb71e33bu,0xbdd26caeu,0x3d6cff2eu,0x3d20fbe6u,0x00000003u,0x3ee9acefu,0x3f38e622u,0xbf050800u,0x40581074u,0x00000002u},2307720487u,0x405c7b00u,1289603814u,{0xc09925b4u,0xbf9e8167u,0xc12ef380u},2093555856u},
 {4294967295u,0x40800000u,{0x3c3846c6u,0xbbf1e33bu,0xbe526caeu,0x3d6cff2eu,0x3d20fbe6u,0x00000003u,0x3ee9acefu,0x3f38e622u,0xbf050800u,0x40581074u,0x00000002u},2307720487u,0x405c7b00u,1289603814u,{0xc09925b4u,0xbf9e8167u,0xc12ef380u},2093555856u},
 {1234567u,0x3f000000u,{0x3b9b6d47u,0x3bbaabf4u,0xbc996b28u,0x3d72cd71u,0x3d3e0335u,0x00000004u,0x3f3a8112u,0xbe8aa6aau,0x3f211400u,0x40c838a8u,0x00000002u},2129541295u,0x406eb280u,1201457998u,{0xc0965efdu,0xc01019aeu,0xc12cf468u},4094321720u},
 {1234567u,0x3f800000u,{0x3c1b6d47u,0x3c3aabf4u,0xbd196b28u,0x3d72cd71u,0x3d3e0335u,0x00000004u,0x3f3a8112u,0xbe8aa6aau,0x3f211400u,0x40c838a8u,0x00000002u},2129541295u,0x406eb280u,1201457998u,{0xc0965efdu,0xc01019aeu,0xc12cf468u},4094321720u},
 {1234567u,0x40000000u,{0x3c9b6d47u,0x3cbaabf4u,0xbd996b28u,0x3d72cd71u,0x3d3e0335u,0x00000004u,0x3f3a8112u,0xbe8aa6aau,0x3f211400u,0x40c838a8u,0x00000002u},2129541295u,0x406eb280u,1201457998u,{0xc0965efdu,0xc01019aeu,0xc12cf468u},4094321720u},
 {1234567u,0x40800000u,{0x3d1b6d47u,0x3d3aabf4u,0xbe196b28u,0x3d72cd71u,0x3d3e0335u,0x00000004u,0x3f3a8112u,0xbe8aa6aau,0x3f211400u,0x40c838a8u,0x00000002u},2129541295u,0x406eb280u,1201457998u,{0xc0965efdu,0xc01019aeu,0xc12cf468u},4094321720u},
 {7654321u,0x3f000000u,{0x3d5ac677u,0xbd55a262u,0x3d746dd5u,0x3e0ac6a8u,0x3edaf0a4u,0x00000004u,0x3f19de72u,0xbf4af591u,0xbdcec000u,0x40c17736u,0x00000002u},1985266201u,0x405a8180u,3685273808u,{0xbf5d3d83u,0xc0427c2du,0x4067f4c2u},1219192266u},
 {7654321u,0x3f800000u,{0x3ddac677u,0xbdd5a262u,0x3df46dd5u,0x3e0ac6a8u,0x3edaf0a4u,0x00000004u,0x3f19de72u,0xbf4af591u,0xbdcec000u,0x40c17736u,0x00000002u},1985266201u,0x405a8180u,3685273808u,{0xbf5d3d83u,0xc0427c2du,0x4067f4c2u},1219192266u},
 {7654321u,0x40000000u,{0x3e5ac677u,0xbe55a262u,0x3e746dd5u,0x3e0ac6a8u,0x3edaf0a4u,0x00000004u,0x3f19de72u,0xbf4af591u,0xbdcec000u,0x40c17736u,0x00000002u},1985266201u,0x405a8180u,3685273808u,{0xbf5d3d83u,0xc0427c2du,0x4067f4c2u},1219192266u},
 {7654321u,0x40800000u,{0x3edac677u,0xbed5a262u,0x3ef46dd5u,0x3e0ac6a8u,0x3edaf0a4u,0x00000004u,0x3f19de72u,0xbf4af591u,0xbdcec000u,0x40c17736u,0x00000002u},1985266201u,0x405a8180u,3685273808u,{0xbf5d3d83u,0xc0427c2du,0x4067f4c2u},1219192266u},
};
int main(void)
{
    uint32_t i;rf_random_state rng;rf_geomod_debris_birth_result birth,before;rf_geomod_debris_mesh mesh;
    float radius,velocity[3],zero[3]={0};
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        rng.value=cases[i].seed;memcpy(&radius,&cases[i].radius,4);
        CHECK(rf_geomod_debris_birth(radius,&rng,&birth)==RF_OK);
        if(memcmp(&birth,cases[i].birth,sizeof(birth))){fprintf(stderr,"original birth fixture %u mismatch\n",i);return 1;}
        CHECK(rng.value==cases[i].state8);
        CHECK(rf_geomod_debris_build(birth.radius,256,256,&rng,&mesh)==RF_OK);
        CHECK(rng.value==cases[i].state33 && !memcmp(&mesh.lifetime,&cases[i].lifetime,4));
        CHECK(rf_geomod_debris_launch(birth.displacement,zero,birth.radius,birth.resistance,&rng,velocity)==RF_OK);
        CHECK(rng.value==cases[i].state35 && !memcmp(velocity,cases[i].velocity,12));
    }
    memset(&before,0xa5,sizeof(before));
    for(i=0;i<4;i++) {
        const float invalid[4]={0,-1,NAN,INFINITY};birth=before;rng.value=123;
        CHECK(rf_geomod_debris_birth(invalid[i],&rng,&birth)!=RF_OK);
        CHECK(rng.value==123 && !memcmp(&birth,&before,sizeof(birth)));
    }
    CHECK(rf_geomod_debris_birth(1,NULL,&birth)==RF_RANGE);
    CHECK(rf_geomod_debris_birth(1,&rng,NULL)==RF_RANGE);
    puts("PASS20 original35-draw births, exact count/lifetime/order, placement and resistance");return 0;
}
