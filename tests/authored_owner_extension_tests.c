#include "rf/authored_owner_extension.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int decode_reject(const void *p,uint32_t n,const rf_authored_owner_expected *e,uint32_t cuts)
{rf_authored_owner_extension out,old;memset(&out,0xa5,sizeof(out));memcpy(&old,&out,sizeof(old));
 return rf_authored_owner_extension_decode(p,n,e,cuts,&out)!=RF_OK && !memcmp(&out,&old,sizeof(out));}
static int encode_reject(const rf_authored_owner_extension *v,const rf_authored_owner_expected *e,uint32_t cuts,uint32_t n)
{unsigned char out[129],old[129];memset(out,0xa5,sizeof(out));memcpy(old,out,sizeof(old));
 return rf_authored_owner_extension_encode(v,e,cuts,out,n)!=RF_OK && !memcmp(out,old,sizeof(out));}
int main(void)
{
    static const unsigned char words[32]={94,0,0,0, 0,0,0,0, 6,0,0,0, 3,0,0,0,
        1,0,0,0, 1,0,0,0, 1,0,0,0, 4,0,0,0};
    rf_authored_owner_extension value={0},decoded;rf_authored_owner_expected expected={0};
    unsigned char encoded[128],vector[128],bad[129];uint32_t i;
    value.uid=expected.uid=94;value.source_count=expected.source_count=6;value.neighbor_count=expected.neighbor_count=3;
    value.publication_policy=value.collision_policy=value.material_policy=1;value.serial=4;
    for(i=0;i<32;i++) {
        value.publication_digest[i]=expected.publication_digest[i]=(unsigned char)i;
        value.collision_digest[i]=expected.collision_digest[i]=(unsigned char)(32+i);
        value.material_digest[i]=expected.material_digest[i]=(unsigned char)(64+i);
    }
    memcpy(vector,words,32);for(i=0;i<96;i++)vector[32+i]=(unsigned char)i;
    CHECK(!rf_authored_owner_extension_encode(&value,&expected,2,encoded,128) && !memcmp(encoded,vector,128));
    CHECK(!rf_authored_owner_extension_decode(vector,128,&expected,2,&decoded));
    CHECK(decoded.uid==94 && decoded.mode==0 && decoded.source_count==6 && decoded.neighbor_count==3 && decoded.serial==4);
    CHECK(!rf_authored_owner_extension_encode(&decoded,&expected,2,encoded,128) && !memcmp(encoded,vector,128));
    for(i=0;i<128;i++){CHECK(decode_reject(vector,i,&expected,2));CHECK(encode_reject(&value,&expected,2,i));}
    memcpy(bad,vector,128);bad[128]=0;CHECK(decode_reject(bad,129,&expected,2));CHECK(encode_reject(&value,&expected,2,129));
    for(i=0;i<96;i++){memcpy(bad,vector,128);bad[32+i]^=1;CHECK(decode_reject(bad,128,&expected,2));}
    for(i=0;i<7;i++){memcpy(bad,vector,128);bad[i*4]^=2;CHECK(decode_reject(bad,128,&expected,2));}
    memcpy(bad,vector,128);memset(bad+28,255,4);CHECK(decode_reject(bad,128,&expected,2));
    memset(bad+28,0,4);CHECK(decode_reject(bad,128,&expected,2));
    CHECK(decode_reject(vector,128,&expected,5));CHECK(decode_reject(vector,128,&expected,9));
    CHECK(decode_reject(NULL,128,&expected,2));CHECK(decode_reject(vector,128,NULL,2));
    CHECK(encode_reject(NULL,&expected,2,128));CHECK(encode_reject(&value,NULL,2,128));
    expected.uid++;CHECK(decode_reject(vector,128,&expected,2));expected.uid--;
    expected.source_count=3;CHECK(decode_reject(vector,128,&expected,2));expected.source_count=6;
    expected.neighbor_count=0;CHECK(decode_reject(vector,128,&expected,2));expected.neighbor_count=3;
    expected.material_digest[0]^=1;CHECK(encode_reject(&value,&expected,2,128));expected.material_digest[0]^=1;
    value.mode=1;CHECK(encode_reject(&value,&expected,2,128));value.mode=0;
    value.publication_policy=2;CHECK(encode_reject(&value,&expected,2,128));value.publication_policy=1;
    value.serial=UINT32_MAX;CHECK(encode_reject(&value,&expected,2,128));
    value.serial=UINT32_MAX-1;CHECK(!rf_authored_owner_extension_encode(&value,&expected,8,encoded,128));
    CHECK(!rf_authored_owner_extension_decode(encoded,128,&expected,8,&decoded) && decoded.serial==UINT32_MAX-1);
    value.serial=0;CHECK(!rf_authored_owner_extension_encode(&value,&expected,0,encoded,128));
    CHECK(!rf_authored_owner_extension_decode(encoded,128,&expected,0,&decoded) && !decoded.serial);
    value.serial=3;CHECK(!rf_authored_owner_extension_encode(&value,&expected,0,encoded,128));
    CHECK(!rf_authored_owner_extension_decode(encoded,128,&expected,0,&decoded) && decoded.serial==3);
    CHECK(encode_reject(&value,&expected,4,128));
    puts("PASS authored owner extension exact128 LE vector, identity/policy gates, serial/reset, atomic failures");return 0;
}
