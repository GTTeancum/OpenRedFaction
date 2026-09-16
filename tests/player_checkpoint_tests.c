#include "rf/player_checkpoint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define CHECK(v) do{if(!(v)){fprintf(stderr,"RFPL line%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static void put(unsigned char *p,uint32_t n){unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static void reject(const unsigned char *p,uint32_t n,const rf_player_checkpoint_catalog *c)
{rf_player_checkpoint out,before;memset(&out,0xa5,sizeof(out));before=out;CHECK(rf_player_checkpoint_decode(p,n,c,&out)!=RF_OK);CHECK(!memcmp(&out,&before,sizeof(out)));}
int main(void)
{
 rf_player_checkpoint_catalog c={0},bad_catalog;rf_player_checkpoint v={0},decoded,bad;unsigned char bytes[545],changed[545],encoded[544];
 uint32_t i,j;float body[9],eye[9],saved[9],pi2=1.5707963705062866f,tau=6.2831854820251465f;
 c.hash=0x12345678;c.count=64;c.health_capacity=100;c.armor_capacity=100;
 c.supported[2]=c.supported[63]=1;c.weapons[2]=(rf_weapon_acquire_definition){0,300,12};c.weapons[63]=(rf_weapon_acquire_definition){31,400,7};c.reserve_capacity[0]=300;c.reserve_capacity[31]=400;
 v.player.catalog_hash=c.hash;v.player.weapon=63;v.player.health=100;v.player.armor=50;v.player.inventory.owned[2]=v.player.inventory.owned[63]=1;
 v.player.inventory.loaded[2]=3;v.player.inventory.loaded[63]=7;v.player.inventory.reserve[0]=123;v.player.inventory.reserve[31]=400;
 v.position[0]=12.5f;v.position[1]=-3;v.position[2]=9;v.body_angles[1]=1;v.eye_angles[0]=.25f;
 CHECK(!rf_player_checkpoint_encode(&v,&c,bytes,544));CHECK(!memcmp(bytes,"RFPL\1\0\0\0\x20\x02\0\0",12));
 CHECK(bytes[16]==0x78&&bytes[17]==0x56&&bytes[18]==0x34&&bytes[19]==0x12&&bytes[20]==63&&bytes[82]==1&&bytes[143]==1);
 CHECK(bytes[268]==0x90&&bytes[269]==1&&bytes[524]==7);CHECK(!rf_player_checkpoint_decode(bytes,544,&c,&decoded));
 CHECK(!rf_player_checkpoint_encode(&decoded,&c,encoded,544)&&!memcmp(bytes,encoded,544));CHECK(decoded.player.weapon==63&&decoded.player.inventory.loaded[2]==3&&decoded.position[0]==12.5f);
 CHECK(!rf_player_checkpoint_validate(&decoded,&c,body,eye));CHECK(fabsf(body[4]-1)<1e-6f&&fabsf(body[7])<1e-6f);CHECK(fabsf(eye[7])>.1f);
 for(i=0;i<544;i++)reject(bytes,i,&c);memcpy(changed,bytes,544);changed[544]=0;reject(changed,545,&c);
 for(i=0;i<4;i++){memcpy(changed,bytes,544);put(changed+i*4,0xffffffff);reject(changed,544,&c);}
 for(i=68;i<80;i++){memcpy(changed,bytes,544);changed[i]=1;reject(changed,544,&c);}
 for(i=528;i<544;i++){memcpy(changed,bytes,544);changed[i]=1;reject(changed,544,&c);}
 {const uint32_t offsets[]={24,28,32,36,40,44,48,52,56,60,64};for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++)for(j=0;j<2;j++){memcpy(changed,bytes,544);put(changed+offsets[i],j?0x7f800000:0x7fc00000);reject(changed,544,&c);}}
 memcpy(changed,bytes,544);changed[80+2]=2;reject(changed,544,&c);
 memcpy(changed,bytes,544);changed[80+1]=1;reject(changed,544,&c);
 memcpy(changed,bytes,544);changed[80+63]=0;reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+20,64);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+20,1);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+16,0);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+144,0xffffffff);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+272,0xffffffff);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+268,401);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+524,8);reject(changed,544,&c);
 memcpy(changed,bytes,544);put(changed+148,1);reject(changed,544,&c);
 for(i=0;i<4;i++) {bad=v;if(i==0)bad.player.health=0;if(i==1)bad.player.health=101;if(i==2)bad.player.armor=-1;if(i==3)bad.player.armor=101;memset(encoded,0xa5,544);CHECK(rf_player_checkpoint_encode(&bad,&c,encoded,544)==RF_FORMAT);for(j=0;j<544;j++)CHECK(encoded[j]==0xa5);}
 bad=v;bad.player.weapon=UINT32_MAX;CHECK(!rf_player_checkpoint_encode(&bad,&c,encoded,544));CHECK(!rf_player_checkpoint_decode(encoded,544,&c,&decoded)&&decoded.player.weapon==UINT32_MAX); /* Preserve intentionally unarmed even with owned weapons. */
 for(i=0;i<4;i++){bad=v;bad.body_angles[1]=(i&1)?tau:-tau;bad.eye_angles[0]=(i&2)?pi2:-pi2;CHECK(!rf_player_checkpoint_validate(&bad,&c,body,eye));}
 bad=v;bad.body_angles[1]=nextafterf(tau,INFINITY);CHECK(rf_player_checkpoint_validate(&bad,&c,NULL,NULL)==RF_FORMAT);
 bad=v;bad.eye_angles[0]=nextafterf(pi2,INFINITY);CHECK(rf_player_checkpoint_validate(&bad,&c,NULL,NULL)==RF_FORMAT);
 bad=v;bad.body_angles[0]=.1f;for(i=0;i<9;i++)body[i]=eye[i]=saved[i]=123;
 CHECK(rf_player_checkpoint_validate(&bad,&c,body,eye)==RF_FORMAT&&!memcmp(body,saved,sizeof(body))&&!memcmp(eye,saved,sizeof(eye)));
 bad=v;bad.position[0]=FLT_MAX;CHECK(!rf_player_checkpoint_validate(&bad,&c,NULL,NULL)); /* Finite only: body fit remains caller-owned. */
 for(i=0;i<6;i++){bad_catalog=c;if(i==0)bad_catalog.count=0;if(i==1)bad_catalog.count=65;if(i==2)bad_catalog.supported[0]=2;if(i==3)bad_catalog.weapons[63].ammo_type=32;if(i==4)bad_catalog.reserve_capacity[31]=-1;if(i==5)bad_catalog.health_capacity=NAN;CHECK(rf_player_checkpoint_validate(&v,&bad_catalog,NULL,NULL)==RF_RANGE);}
 bad_catalog=c;bad_catalog.count=63;CHECK(rf_player_checkpoint_validate(&v,&bad_catalog,NULL,NULL)==RF_RANGE);
 CHECK(rf_player_checkpoint_encode(&v,&c,bytes,543)==RF_RANGE);CHECK(rf_player_checkpoint_decode(NULL,544,&c,&decoded)==RF_RANGE);
 puts("PASS RFPL roundtrip/truncation/pose/catalog/ammo/rollback boundaries");return 0;
}
