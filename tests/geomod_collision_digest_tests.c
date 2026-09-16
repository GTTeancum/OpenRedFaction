#include "rf/geomod_collision_digest.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int rejected(rf_geomod_collision_digest_input *v)
{unsigned char out[32],old[32];memset(out,0xa5,32);memcpy(old,out,32);return rf_geomod_collision_digest(v,out)!=RF_OK&&!memcmp(out,old,32);}
int main(void)
{
    /* Independent Python hashlib532-byte RFAC fixture, nested RFCM digest. */
    static const unsigned char expected[32]={0x5c,0x6e,0xa2,0x29,0x0d,0x41,0x23,0x6b,0x45,0x83,0xe7,0x0b,0xcd,0x0f,0x22,0x0f,0xa7,0x81,0x2e,0x77,0xc3,0x51,0x1e,0x04,0x08,0x16,0x5a,0xc5,0x3c,0x53,0xcf,0xde};
    float points[3][3][3]={{{0,0,0},{1,0,0},{0,1,0}},{{0,0,1},{1,0,1},{0,1,1}},{{0,0,2},{1,0,2},{0,1,2}}};
    rf_collision_face canonical_faces[3]={0},faces[3];uint32_t permutation[3]={2,0,1},ids[3]={141,149,149},stack=0x12345678,i;
    rf_collision_tree tree={0};rf_collision_composition_view composed={0};
    rf_geomod_collision_digest_row rows[3]={0};rf_geomod_digest_material material={0};
    rf_geomod_collision_digest_input in={0};unsigned char source[32],pixel[4]={1,2,3,4},digest[32],again[32];
    for(i=0;i<32;i++)source[i]=(unsigned char)i;
    for(i=0;i<3;i++) {
        rf_collision_face *f=canonical_faces+i;f->plane[2]=1;f->plane[3]=-(float)i;
        f->minimum[2]=f->maximum[2]=(float)i;f->maximum[0]=f->maximum[1]=1;f->vertices=points[i];f->count=3;
        f->filter.face_flags=i?256:0;f->filter.property_34=-1;f->filter.owner_present=1;f->triangle_surface=i?1:0;
        rows[i].canonical_order=i;rows[i].domain=i?RF_GEOMOD_COLLISION_PUBLISHED:RF_GEOMOD_COLLISION_AUTHORED;
        rows[i].fragment=i?i-1:0;rows[i].material=7;
        rows[i].origin=(rf_geomod_publication_origin){i?1:0,i?94:71,i?UINT32_MAX:415,i?149:141};
    }
    for(i=0;i<3;i++)faces[i]=canonical_faces[permutation[i]];
    tree.faces=faces;tree.source_indices=permutation;tree.face_count=3;tree.stack=&stack;
    composed.tree=&tree;composed.face_ids=ids;composed.count=3;
    material.key=7;strcpy(material.image.name,"rock.tga");material.image.width=material.image.height=1;
    material.image.format=4;material.image.bytes_per_pixel=material.image.bytes=4;material.image.pixels=pixel;
    in.composition=&composed;in.rows=rows;in.row_count=3;in.materials=&material;in.material_count=1;
    in.source_identity=source;in.room=3;in.collision_policy=1;
    CHECK(!rf_geomod_collision_digest(&in,digest));
    CHECK(!memcmp(digest,expected,32));
    printf("CANONICAL ");for(i=0;i<32;i++)printf("%02x",digest[i]);puts("");
    {static const unsigned char resource[32]={0xa9,0x27,0xb5,0xb9,0x12,0xd8,0x07,0x85,0x8f,0x44,0xf0,0xd1,0xc4,0x30,0x61,0x07,
        0x2a,0xae,0xa2,0xdd,0xa3,0x3b,0x86,0xdb,0xe2,0xa1,0x37,0x65,0x95,0xbb,0x0f,0xe7};
     memcpy(material.content_digest,resource,32);material.prehashed=1;material.image.pixels=NULL;
     CHECK(!rf_geomod_collision_digest(&in,again)&&!memcmp(digest,again,32));
     material.image.pixels=pixel;CHECK(rejected(&in));material.prehashed=0;}
    /* BVH permutation changes only: semantic source order is unchanged. */
    {rf_collision_face f=faces[0];uint32_t p=permutation[0];faces[0]=faces[2];faces[2]=f;permutation[0]=permutation[2];permutation[2]=p;
     CHECK(!rf_geomod_collision_digest(&in,again)&&!memcmp(digest,again,32));
     f=faces[0];p=permutation[0];faces[0]=faces[2];faces[2]=f;permutation[0]=permutation[2];permutation[2]=p;}
    /* Composition source-order storage itself can relocate; canonical_order
     * and the correctly joined permutation restore the same semantic stream. */
    {rf_geomod_collision_digest_row old[3];uint32_t oldids[3];memcpy(old,rows,sizeof(rows));memcpy(oldids,ids,sizeof(ids));
     rows[0]=old[2];rows[1]=old[0];rows[2]=old[1];ids[0]=oldids[2];ids[1]=oldids[0];ids[2]=oldids[1];
     for(i=0;i<3;i++)permutation[i]=(permutation[i]+1)%3;
     CHECK(!rf_geomod_collision_digest(&in,again)&&!memcmp(digest,again,32));
     memcpy(rows,old,sizeof(rows));memcpy(ids,oldids,sizeof(ids));for(i=0;i<3;i++)permutation[i]=(permutation[i]+2)%3;}
    material.key=999;for(i=0;i<3;i++)rows[i].material=999;
    rows[0].origin.reference=ids[0]=100;composed.generation=999;tree.allocated_bytes=123;
    CHECK(!rf_geomod_collision_digest(&in,again)&&!memcmp(digest,again,32));
    material.key=7;for(i=0;i<3;i++)rows[i].material=7;rows[0].origin.reference=ids[0]=141;
    CHECK(stack==0x12345678);
    rows[0].domain=RF_GEOMOD_COLLISION_COMPILED;rows[0].origin.owner=3;rows[0].origin.source_face=141;
    CHECK(!rf_geomod_collision_digest(&in,again)&&memcmp(digest,again,32));
    rows[0].domain=RF_GEOMOD_COLLISION_AUTHORED;rows[0].origin.owner=71;rows[0].origin.source_face=415;
    /* An unchanged compiled room needs no static pixels, including animated
     * liquid materials. Its compiled index is qualified by source identity. */
    {rf_geomod_collision_digest_row saved[3];memcpy(saved,rows,sizeof(rows));
     for(i=0;i<3;i++){rows[i].domain=RF_GEOMOD_COLLISION_COMPILED;rows[i].fragment=0;
         rows[i].origin.kind=RF_GEOMOD_PUBLICATION_RETAINED;rows[i].origin.owner=3;
         rows[i].origin.source_face=rows[i].origin.reference=ids[i];rows[i].material=28;}
     /* Distinct compiled ownership, unlike published fragments. */
     ids[2]=rows[2].origin.reference=rows[2].origin.source_face=150;
     in.materials=NULL;in.material_count=0;
     CHECK(!rf_geomod_collision_digest(&in,again));
     rows[0].material=UINT32_MAX;CHECK(rejected(&in));
     memcpy(rows,saved,sizeof(rows));ids[2]=149;in.materials=&material;in.material_count=1;}
    pixel[0]^=1;CHECK(!rf_geomod_collision_digest(&in,again)&&memcmp(digest,again,32));pixel[0]^=1;
    source[0]^=1;CHECK(!rf_geomod_collision_digest(&in,again)&&memcmp(digest,again,32));source[0]^=1;
    faces[0].filter.face_flags^=4;CHECK(!rf_geomod_collision_digest(&in,again)&&memcmp(digest,again,32));faces[0].filter.face_flags^=4;
    faces[0].maximum[0]=2;CHECK(!rf_geomod_collision_digest(&in,again)&&memcmp(digest,again,32));faces[0].maximum[0]=1;
    rows[2].fragment=0;CHECK(rejected(&in));rows[2].fragment=1;
    rows[1].canonical_order=0;CHECK(rejected(&in));rows[1].canonical_order=1;
    permutation[0]=permutation[1];CHECK(rejected(&in));permutation[0]=2;
    permutation[0]=3;CHECK(rejected(&in));permutation[0]=2;
    ids[0]=999;CHECK(rejected(&in));ids[0]=141;
    rows[0].material=8;CHECK(rejected(&in));rows[0].material=7;
    rows[0].fragment=UINT32_MAX;CHECK(rejected(&in));rows[0].fragment=0;
    points[0][0][0]=NAN;CHECK(rejected(&in));points[0][0][0]=0;
    faces[0].minimum[0]=2;CHECK(rejected(&in));faces[0].minimum[0]=0;
    composed.count=2;CHECK(rejected(&in));composed.count=3;
    in.collision_policy=2;CHECK(rejected(&in));in.collision_policy=1;
    CHECK(!rf_geomod_collision_digest(&in,again)&&!memcmp(digest,again,32));
    CHECK(stack==0x12345678);
    puts("PASS full collision digest canonical order, BVH/source permutation, distinct fragments, identity/filter geometry, atomic failure");return 0;
}
