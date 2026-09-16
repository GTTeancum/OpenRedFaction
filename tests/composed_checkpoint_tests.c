#include "rf/composed_checkpoint.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RFCP line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char terrain[RF_CHECKPOINT_FILE_MAX],encoded[RF_CHECKPOINT_FILE_MAX+1],changed[RF_CHECKPOINT_FILE_MAX+1],saved[RF_CHECKPOINT_FILE_MAX+1];
static void put(unsigned char *p,uint32_t n){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void make_terrain(uint32_t bytes)
{uint32_t i;for(i=0;i<bytes;i++)terrain[i]=(unsigned char)(i*37);memcpy(terrain,"RFDS",4);put(terrain+4,1);put(terrain+8,bytes);}
static void reject(const void *p,uint32_t n,uint32_t profile,const rf_player_checkpoint_catalog *catalog)
{
 rf_composed_checkpoint out,kept;memset(&out,0xa5,sizeof(out));memcpy(&kept,&out,sizeof(out));
 CHECK(rf_composed_checkpoint_preflight(p,n,profile,catalog,&out)!=RF_OK);CHECK(!memcmp(&out,&kept,sizeof(out)));
}
static void encode_reject(const rf_player_checkpoint *player,const rf_player_checkpoint_catalog *catalog,uint32_t n,uint32_t capacity)
{
 uint32_t written=0xabcdef12;memset(changed,0xa5,sizeof(changed));memcpy(saved,changed,sizeof(saved));
 CHECK(rf_composed_checkpoint_encode(RF_COMPOSED_PROFILE_CAVITY,player,catalog,terrain,n,changed,capacity,&written)!=RF_OK);
 CHECK(written==0xabcdef12&&!memcmp(changed,saved,sizeof(saved)));
}
int main(void)
{
 rf_player_checkpoint_catalog c={0},bad_catalog;rf_player_checkpoint p={0},bad;rf_composed_checkpoint out;uint32_t n,i,j,written;
 const uint32_t profile=RF_COMPOSED_PROFILE_CAVITY,rfds_at=32+RF_PLAYER_CHECKPOINT_BYTES;
 c.hash=0x87654321;c.count=2;c.supported[1]=1;c.weapons[1]=(rf_weapon_acquire_definition){0,100,10};c.reserve_capacity[0]=100;c.health_capacity=c.armor_capacity=100;
 p.player.catalog_hash=c.hash;p.player.health=37.5f;p.player.armor=12.25f;p.player.weapon=1;p.player.inventory.owned[1]=1;p.player.inventory.loaded[1]=3;p.player.inventory.reserve[0]=17;p.position[0]=1.5f;p.body_angles[1]=.25f;
 make_terrain(288);memset(encoded,0xa5,sizeof(encoded));CHECK(!rf_composed_checkpoint_encode(profile,&p,&c,terrain,288,encoded,sizeof(encoded),&n));CHECK(n==864);
 CHECK(!memcmp(encoded,"RFCP\1\0\0\0\x60\x03\0\0",12));CHECK(encoded[16]==1&&encoded[17]==0&&encoded[18]==0&&encoded[19]==0);CHECK(encoded[n]==0xa5);
 CHECK(!rf_composed_checkpoint_preflight(encoded,n,profile,&c,&out));CHECK(out.rfds==encoded+rfds_at&&out.rfds_bytes==288&&!memcmp(out.rfds,terrain,288));
 CHECK(out.player.player.weapon==1&&out.player.player.health==37.5f&&out.player.player.inventory.loaded[1]==3&&out.player.position[0]==1.5f);
 /* RFDS payload deliberately is not semantically valid; parser must not claim it is. */
 CHECK(!rf_composed_checkpoint_encode(profile,&out.player,&c,out.rfds,out.rfds_bytes,changed,sizeof(changed),&written)&&written==n&&!memcmp(encoded,changed,n));
 for(i=0;i<n;i++)reject(encoded,i,profile,&c);reject(encoded,n+1,profile,&c);reject(encoded,n,profile+1,&c);
 for(i=0;i<8;i++){
  memcpy(changed,encoded,n);put(changed+i*4,0xffffffff);reject(changed,n,profile,&c);
 }
 /* Corrupt nested RFPL boundaries, catalog, pose and ammo without changing envelope. */
 {const uint32_t offsets[]={0,4,8,12,16,20,24,32,144,272};for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){
  memcpy(changed,encoded,n);put(changed+32+offsets[i],offsets[i]==20?2:0xffffffff);reject(changed,n,profile,&c);
 }}
 for(i=0;i<3;i++){memcpy(changed,encoded,n);put(changed+rfds_at+i*4,0xffffffff);reject(changed,n,profile,&c);}
 bad=p;bad.player.health=NAN;encode_reject(&bad,&c,288,sizeof(changed));bad_catalog=c;bad_catalog.count=0;encode_reject(&p,&bad_catalog,288,sizeof(changed));
 encode_reject(&p,&c,288,863);encode_reject(&p,&c,287,sizeof(changed));
 for(i=0;i<3;i++){make_terrain(288);put(terrain+i*4,0xffffffff);encode_reject(&p,&c,288,sizeof(changed));}
 make_terrain(RF_COMPOSED_CHECKPOINT_RFDS_MAX);CHECK(!rf_composed_checkpoint_encode(profile,&p,&c,terrain,RF_COMPOSED_CHECKPOINT_RFDS_MAX,encoded,sizeof(encoded),&n)&&n==RF_CHECKPOINT_FILE_MAX);
 CHECK(!rf_composed_checkpoint_preflight(encoded,n,profile,&c,&out)&&out.rfds_bytes==RF_COMPOSED_CHECKPOINT_RFDS_MAX&&!memcmp(out.rfds,terrain,out.rfds_bytes));
 for(i=0;i<8;i++){uint32_t shorter=n-1-i;memcpy(changed,encoded,n);put(changed+8,shorter);put(changed+24,shorter-rfds_at);reject(changed,shorter,profile,&c);} /* Embedded RFDS length guards. */
 make_terrain(RF_COMPOSED_CHECKPOINT_RFDS_MAX+1);encode_reject(&p,&c,RF_COMPOSED_CHECKPOINT_RFDS_MAX+1,sizeof(changed));
 make_terrain(RF_CHECKPOINT_FILE_MAX);encode_reject(&p,&c,RF_CHECKPOINT_FILE_MAX,sizeof(changed));
 make_terrain(288);reject(terrain,288,profile,&c); /* Legacy RFDS is caller dispatch policy. */
 p.player.weapon=UINT32_MAX;CHECK(!rf_composed_checkpoint_encode(profile,&p,&c,terrain,288,encoded,sizeof(encoded),&n));CHECK(!rf_composed_checkpoint_preflight(encoded,n,profile,&c,&out)&&out.player.player.weapon==UINT32_MAX&&out.player.player.inventory.owned[1]);
 memcpy(saved,encoded,sizeof(saved));reject(NULL,n,profile,&c);written=0xabcdef12;CHECK(rf_composed_checkpoint_encode(profile,&p,&c,NULL,288,encoded,sizeof(encoded),&written)==RF_RANGE&&written==0xabcdef12&&!memcmp(encoded,saved,sizeof(encoded)));
 for(j=0;j<2;j++)CHECK(rf_composed_checkpoint_encode(profile,&p,&c,terrain,288,j?encoded:NULL,sizeof(encoded),j?NULL:&written)==RF_RANGE);
 memcpy(changed+rfds_at,terrain,288);CHECK(!rf_composed_checkpoint_encode(profile,&p,&c,changed+rfds_at,288,changed,sizeof(changed),&written));
 CHECK(written==n&&!memcmp(changed,encoded,n));
 memcpy(saved,changed,sizeof(saved));bad=p;bad.player.health=NAN;written=0xabcdef12;
 CHECK(rf_composed_checkpoint_encode(profile,&bad,&c,changed+rfds_at,288,changed,sizeof(changed),&written)==RF_FORMAT&&written==0xabcdef12&&!memcmp(changed,saved,sizeof(saved)));
 /* Authored profile2 framing is distinct; this envelope never claims that
  * terrain identity, publication, maps or player placement are validated. */
 make_terrain(416);put(terrain+4,2);put(terrain+276,416);put(terrain+280,128);put(terrain+284,2);
 CHECK(!rf_composed_checkpoint_encode(RF_COMPOSED_PROFILE_AUTHORED,&p,&c,terrain,416,encoded,sizeof(encoded),&n));
 CHECK(n==rfds_at+416&&!rf_composed_checkpoint_preflight(encoded,n,RF_COMPOSED_PROFILE_AUTHORED,&c,&out));
 CHECK(out.rfds_bytes==416&&!memcmp(out.rfds,terrain,416));
 reject(encoded,n,profile,&c);reject(encoded,n,0,&c);reject(encoded,n,3,&c);
 for(i=0;i<4;i++) {
  const uint32_t offsets[]={4,276,280,284};memcpy(changed,encoded,n);
  put(changed+rfds_at+offsets[i],i?0:1);reject(changed,n,RF_COMPOSED_PROFILE_AUTHORED,&c);
 }
 /* Crossed version and unsupported-profile encode must preserve all bytes. */
 for(i=0;i<3;i++) {
  uint32_t selected=i==0?profile:i==1?0:3;
  memset(changed,0xa5,sizeof(changed));memcpy(saved,changed,sizeof(saved));written=0xabcdef12;
  CHECK(rf_composed_checkpoint_encode(selected,&p,&c,terrain,416,changed,sizeof(changed),&written)==RF_FORMAT);
  CHECK(written==0xabcdef12&&!memcmp(changed,saved,sizeof(saved)));
 }
 /* Version2 cannot exploit the smaller legacy minimum. */
 memcpy(changed,encoded,n);put(changed+8,rfds_at+415);put(changed+24,415);put(changed+rfds_at+8,415);
 reject(changed,rfds_at+415,RF_COMPOSED_PROFILE_AUTHORED,&c);
 puts("PASS RFCP envelope/player preflight, borrowed unchanged RFDS, truncation/malformed/profile/cap/rollback; authored version dispatch");return 0;
}
