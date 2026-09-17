#include "rf/authored_identity_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    static const char *names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
    /* Authored reconstruction policy8; earlier saves have a different identity. */
    static const unsigned char known[32]={0x9f,0x7a,0x49,0x0e,0x6c,0x30,0x26,0x11,0xd8,0xbf,0x32,0x93,0xb5,0x9d,0xc4,0x70,
        0xc8,0xd1,0x7f,0x6f,0x8f,0xb8,0x89,0xf3,0xaa,0xcc,0x2a,0x0b,0x5a,0x7a,0xe8,0xb4};
    rf_vpp archive={0},maps[6]={{0}};rf_level level;rf_geometry geometry={0};rf_lightmap_rgb_owner rgb={0};
    rf_geomod_authored_post *owner=NULL;rf_geomod_authored_post_view asset;
    rf_geomod_digest_material materials[8],old_materials[8],substrate,old_substrate;
    rf_geomod_authored_chart_identity references[32],old_references[32];
    rf_geomod_authored_identity_manifest manifest={0},old_manifest;unsigned char digest[32],old_digest[32],again[32];
    uint32_t peak=0,old_peak,i;char path[1024];rf_geomod_digest_chart charts[32];uint32_t face_charts[256];
    rf_geomod_publication_digest_input publication={0};
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
    CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_lightmap_rgb_open(&rgb,&level,16*1024*1024));
    CHECK(!rf_geomod_authored_post_open(&level,&geometry,2*1024*1024,&owner));CHECK(!rf_geomod_authored_post_get(owner,&asset));
    for(i=0;i<6;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);CHECK(!rf_vpp_open(maps+i,path));}
    memset(materials,0xa5,sizeof(materials));memset(references,0xa5,sizeof(references));
    memset(&substrate,0xa5,sizeof(substrate));manifest.substrate=&substrate;
    manifest.materials=materials;manifest.material_capacity=8;manifest.references=references;manifest.reference_capacity=32;
    CHECK(!rf_geomod_authored_identity_capture_manifest(&level,&geometry,&asset,maps,6,&rgb,2*1024*1024,digest,&peak,&manifest));
    if(memcmp(digest,known,32)){fprintf(stderr,"SOURCE_IDENTITY ");for(i=0;i<32;i++)fprintf(stderr,"%02x",digest[i]);fprintf(stderr,"\n");}
    CHECK(!memcmp(digest,known,32) && peak<=2*1024*1024 && manifest.material_count==2 && manifest.reference_count==19);
    CHECK(manifest.resident_bytes==sizeof(manifest)+sizeof(materials)+sizeof(references)+sizeof(substrate));
    CHECK(substrate.key==0 && substrate.prehashed==1 && !substrate.image.pixels &&
        !strcmp(substrate.image.name,"rock02.tga") && substrate.image.width==256 && substrate.image.height==256);
    for(i=0;i<manifest.material_count;i++)CHECK(materials[i].prehashed==1 && !materials[i].image.pixels);
    for(i=0;i<manifest.reference_count;i++) {
        CHECK(references[i].reference==references[i].chart.key && !references[i].chart.image.pixels);
        CHECK(references[i].chart.kind==RF_GEOMOD_DIGEST_UNLIT || references[i].chart.prehashed==1);
        charts[i]=references[i].chart;
    }
    /* Real unmodified source windows consume the manifest without retaining
     * any captured pixel allocation or substituting renderer image slots. */
    CHECK(asset.windows.face_count<=256);for(i=0;i<asset.windows.face_count;i++)face_charts[i]=asset.window_origins[i].reference;
    publication.mesh=asset.windows;publication.origins=asset.window_origins;publication.face_charts=face_charts;
    publication.materials=materials;publication.material_count=manifest.material_count;publication.charts=charts;
    publication.chart_count=manifest.reference_count;publication.publication_policy=publication.material_policy=1;
    CHECK(!rf_geomod_publication_digest(&publication,again));
    printf("INSTALLED_MANIFEST source");for(i=0;i<32;i++)printf("%02x",digest[i]);printf(" windows");for(i=0;i<32;i++)printf("%02x",again[i]);
    printf(" materials%u references%u resident%u peak%u\n",manifest.material_count,manifest.reference_count,manifest.resident_bytes,peak);
    memcpy(&old_substrate,&substrate,sizeof(substrate));memcpy(old_materials,materials,sizeof(materials));memcpy(old_references,references,sizeof(references));
    memcpy(&old_manifest,&manifest,sizeof(manifest));memcpy(old_digest,digest,32);old_peak=peak;
    CHECK(rf_geomod_authored_identity_capture_manifest(&level,&geometry,&asset,maps,6,&rgb,1,digest,&peak,&manifest)!=RF_OK);
    CHECK(!memcmp(&old_substrate,&substrate,sizeof(substrate)) && !memcmp(old_materials,materials,sizeof(materials)) && !memcmp(old_references,references,sizeof(references)) &&
        !memcmp(&old_manifest,&manifest,sizeof(manifest)) && !memcmp(old_digest,digest,32) && old_peak==peak);
    manifest.material_capacity=1;memcpy(&old_manifest,&manifest,sizeof(manifest));
    CHECK(rf_geomod_authored_identity_capture_manifest(&level,&geometry,&asset,maps,6,&rgb,2*1024*1024,digest,&peak,&manifest)==RF_RANGE);
    CHECK(!memcmp(&old_substrate,&substrate,sizeof(substrate)) && !memcmp(old_materials,materials,sizeof(materials)) && !memcmp(old_references,references,sizeof(references)) &&
        !memcmp(&old_manifest,&manifest,sizeof(manifest)) && !memcmp(old_digest,digest,32) && old_peak==peak);
    manifest.material_capacity=8;
    {rf_vpp duplicate[2];rf_vpp_entry entry;uint32_t map;
     for(map=0;map<6;map++)if(!rf_vpp_find(maps+map,materials[0].image.name,&entry))break;
     CHECK(map<6);duplicate[0]=duplicate[1]=maps[map];memcpy(&old_manifest,&manifest,sizeof(manifest));
     CHECK(rf_geomod_authored_identity_capture_manifest(&level,&geometry,&asset,duplicate,2,&rgb,2*1024*1024,digest,&peak,&manifest)==RF_FORMAT);
     CHECK(!memcmp(&old_substrate,&substrate,sizeof(substrate)) && !memcmp(old_materials,materials,sizeof(materials)) && !memcmp(old_references,references,sizeof(references)) &&
        !memcmp(&old_manifest,&manifest,sizeof(manifest)) && !memcmp(old_digest,digest,32) && old_peak==peak);}
    CHECK(!rf_geomod_authored_identity_capture(&level,&geometry,&asset,maps,6,&rgb,2*1024*1024,again,&peak) && !memcmp(again,known,32));
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_geomod_authored_post_close(&owner);rf_lightmap_rgb_close(&rgb);
    rf_geometry_close(&geometry);rf_vpp_close(&archive);
    puts("PASS installed trusted manifest capture, no pixel borrow, real source windows, budget/capacity/ownership rollback, legacy SHA");return 0;
}
