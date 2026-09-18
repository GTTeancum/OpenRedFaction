#include "rf/geomod_authored_identity.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int rejected(rf_geomod_authored_identity_input *in)
{unsigned char out[32],old[32];memset(out,0xa5,32);memcpy(old,out,32);return rf_geomod_authored_identity(in,out)!=RF_OK&&!memcmp(out,old,32);}
int main(void)
{
    /* Independently serialized 706-byte RFAS v1 vector, Python hashlib SHA256. */
    static const unsigned char expected[32]={0xc7,0x09,0x50,0x0e,0xa9,0x08,0xc4,0x91,0xb7,0xdf,0x1e,0x6e,0xe2,0x76,0x81,0xaa,0x70,0xb9,0x64,0x18,0x86,0x5f,0xab,0x5e,0x02,0x82,0x4d,0x8d,0x66,0xd8,0x47,0x4f};
    rf_geomod_vertex vertices[3]={{{0,0,0},{0,0}},{{1,0,0},{1,0}},{{0,1,0},{0,1}}},copy[3];
    rf_geomod_face source={0,3,7,550},window={0,3,7,550},neighbor={0,3,8,415};
    rf_geomod_publication_origin so={0,94,550,149},wo={0,94,550,149},no={2,71,415,141};
    float planes[1][4]={{0,0,1,0}};rf_geomod_publication_solid solid={planes,1,71};
    rf_collision_face_filter filter={0,0,-1,1,0,0};uint32_t replace=149;
    unsigned char pixels[2][4]={{1,2,3,4},{5,6,7,8}},out[32],original[32];uint32_t i;
    rf_geomod_identity_material materials[2]={0};rf_geomod_identity_reference refs[2]={0};
    rf_geomod_authored_post_view asset={0};rf_geomod_authored_identity_input in={0};
    /* Deliberately minimal canonicalization vector, not a topology fixture. */
    asset.source=(rf_geomod_mesh_view){vertices,&source,3,1,1};asset.windows=(rf_geomod_mesh_view){vertices,&window,3,1,2};
    asset.neighbors=(rf_geomod_mesh_view){vertices,&neighbor,3,1,3};asset.source_planes=planes;asset.solids=&solid;
    asset.source_origins=&so;asset.window_origins=&wo;asset.neighbor_origins=&no;asset.source_filters=&filter;
    asset.replaced_ids=&replace;asset.source_uid=94;asset.room=3;asset.solid_count=asset.replaced_count=1;
    asset.brush_count=2;asset.authored_face_count=2;strcpy(asset.settings.texture,"rock02.tga");asset.settings.hardness=100;
    for(i=0;i<2;i++) {
        materials[i].compiled_material=7+i;strcpy(materials[i].image.name,i?"floor.tga":"wood.tga");
        materials[i].image.width=materials[i].image.height=1;materials[i].image.format=4;
        materials[i].image.bytes_per_pixel=materials[i].image.bytes=4;materials[i].image.pixels=pixels[i];
        refs[i].reference=i?141:149;refs[i].owner=i?71:94;refs[i].source_face=i?415:550;
        refs[i].compiled_material=7+i;refs[i].unlit=1;refs[i].filter=filter;
    }
    in.asset=&asset;strcpy(in.level,"ctf06.rfl");in.compiled_section="compiled";in.compiled_bytes=8;
    in.editor_section="editor";in.editor_bytes=6;in.source_operation=2;
    in.loader_policy=in.publication_policy=in.collision_policy=in.material_policy=1;
    in.material_domain=RF_GEOMOD_IDENTITY_COMPILED_MATERIALS;in.materials=materials;in.material_count=2;
    in.references=refs;in.reference_count=2;
    CHECK(!rf_geomod_authored_identity(&in,original));
    CHECK(!memcmp(original,expected,32));
    printf("CANONICAL ");for(i=0;i<32;i++)printf("%02x",original[i]);puts("");
    {
        float hp[1][4]={{0,0,1,0}},copy_hp[1][4];
        rf_geomod_publication_solid holes[2]={{hp,1,71},{hp,1,71}};
        unsigned char hollow[32];
        asset.neighbor_voids=holes;asset.neighbor_void_count=1;
        CHECK(!rf_geomod_authored_identity(&in,hollow) && memcmp(original,hollow,32));
        memcpy(copy_hp,hp,sizeof(hp));holes[0].planes=copy_hp;
        CHECK(!rf_geomod_authored_identity(&in,out) && !memcmp(hollow,out,32));holes[0].planes=hp;
        hp[0][3]=.25f;CHECK(!rf_geomod_authored_identity(&in,out) && memcmp(hollow,out,32));hp[0][3]=0;
        hp[0][2]=-1;CHECK(!rf_geomod_authored_identity(&in,out) && memcmp(hollow,out,32));hp[0][2]=1;
        holes[0].owner=94;CHECK(rejected(&in));holes[0].owner=71;
        asset.neighbor_void_count=2;CHECK(rejected(&in));asset.neighbor_void_count=33;CHECK(rejected(&in));
        asset.neighbor_void_count=1;asset.neighbor_voids=NULL;CHECK(rejected(&in));asset.neighbor_voids=holes;
        holes[0].planes=NULL;CHECK(rejected(&in));holes[0].planes=hp;
        hp[0][3]=NAN;CHECK(rejected(&in));hp[0][3]=0;
        hp[0][2]=2;CHECK(rejected(&in));hp[0][2]=1;
        asset.neighbor_void_count=0;asset.neighbor_voids=NULL;
        CHECK(!rf_geomod_authored_identity(&in,out) && !memcmp(original,out,32));
        puts("PASS hollow source identity: owner/plane sensitivity, pointer relocation, malformed atomic rejection and unchanged legacy v1");
    }
    /* Numeric material/reference keys, pointer and mesh-generation relocation
     * have no effect when the corresponding immutable tables move together. */
    memcpy(copy,vertices,sizeof(copy));asset.source.vertices=copy;asset.source.generation=99;
    source.material=window.material=materials[0].compiled_material=refs[0].compiled_material=70;
    so.reference=wo.reference=replace=refs[0].reference=999;
    CHECK(!rf_geomod_authored_identity(&in,out)&&!memcmp(original,out,32));
    asset.source.vertices=vertices;source.material=window.material=materials[0].compiled_material=refs[0].compiled_material=7;
    so.reference=wo.reference=replace=refs[0].reference=149;
    {rf_geomod_identity_reference tmp=refs[0];refs[0]=refs[1];refs[1]=tmp;
     CHECK(!rf_geomod_authored_identity(&in,out)&&!memcmp(original,out,32));tmp=refs[0];refs[0]=refs[1];refs[1]=tmp;}
    pixels[1][0]^=1;CHECK(!rf_geomod_authored_identity(&in,out)&&memcmp(original,out,32));pixels[1][0]^=1;
    in.editor_section="Editor";CHECK(!rf_geomod_authored_identity(&in,out)&&memcmp(original,out,32));in.editor_section="editor";
    in.publication_policy++;CHECK(!rf_geomod_authored_identity(&in,out)&&memcmp(original,out,32));in.publication_policy--;
    in.reference_count=1;CHECK(rejected(&in));in.reference_count=2;
    refs[1].reference=149;CHECK(rejected(&in));refs[1].reference=141;
    refs[1].owner=95;CHECK(rejected(&in));refs[1].owner=71;
    materials[1].compiled_material=7;CHECK(rejected(&in));materials[1].compiled_material=8;
    in.material_domain=0;CHECK(rejected(&in));in.material_domain=1;
    in.source_mode=2;CHECK(rejected(&in));in.source_mode=0;
    in.source_operation=1;CHECK(rejected(&in));in.source_operation=2;
    in.source_mode=1;CHECK(rejected(&in));in.source_operation=1;
    CHECK(rejected(&in)); /* Cavity profile cannot inherit solid neighbors. */
    in.source_mode=0;in.source_operation=2;
    vertices[0].position[0]=NAN;CHECK(rejected(&in));vertices[0].position[0]=0;
    replace=141;CHECK(rejected(&in));replace=149;
    wo.reference=UINT32_MAX;CHECK(rejected(&in));wo.reference=149;
    refs[0].unlit=0;CHECK(rejected(&in));refs[0].unlit=1;
    materials[0].image.bytes=3;CHECK(rejected(&in));materials[0].image.bytes=4;
    /* Explicit absent hidden source provenance differs from missing a row. */
    so.reference=UINT32_MAX;CHECK(!rf_geomod_authored_identity(&in,out)&&memcmp(original,out,32));so.reference=149;
    refs[0].unlit=0;refs[0].chart=materials[0].image;strcpy(refs[0].chart.name,"ctf06/source-chart");
    refs[0].projection.axes[1]=1;refs[0].projection.scale[0]=refs[0].projection.scale[1]=1;
    CHECK(!rf_geomod_authored_identity(&in,out)&&memcmp(original,out,32));memcpy(original,out,32);
    refs[0].projection.offset[0]=.25f;CHECK(!rf_geomod_authored_identity(&in,out)&&memcmp(original,out,32));
    refs[0].projection.axes[1]=0;CHECK(rejected(&in));
    puts("PASS authored identity key relocation, content sensitivity, malformed preservation");return 0;
}
