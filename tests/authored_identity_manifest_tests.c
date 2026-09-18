#include "rf/authored_identity_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int selected_identities(const rf_level *level,const rf_geometry *geometry,rf_vpp *maps,
    const rf_lightmap_rgb_owner *rgb,const unsigned char known94[32])
{
    static const uint32_t uids[6]={93,94,96,97,95,98};
    unsigned char digests[6][32],repeat[32],windows[6][32];uint32_t n,i,j,peak;
    for(n=0;n<6;n++) {
        rf_geomod_authored_post *owner=NULL;rf_geomod_authored_post_view asset;
        rf_geomod_digest_material materials[8],substrate;
        rf_geomod_authored_chart_identity refs[32];rf_geomod_digest_chart charts[32];
        rf_geomod_authored_identity_manifest manifest={0};
        rf_geomod_publication_digest_input publication={0};uint32_t face_charts[8];
        CHECK(!rf_geomod_authored_post_open_source(level,geometry,uids[n],2*1024*1024,&owner));
        CHECK(!rf_geomod_authored_post_get(owner,&asset));
        manifest.materials=materials;manifest.material_capacity=8;
        manifest.references=refs;manifest.reference_capacity=32;manifest.substrate=&substrate;
        CHECK(!rf_geomod_authored_identity_capture_manifest(level,geometry,&asset,maps,6,rgb,
            2*1024*1024,digests[n],&peak,&manifest));
        CHECK(peak<=2*1024*1024 && manifest.material_count>=1 && manifest.material_count<=8 && manifest.reference_count<=32);
        CHECK(!rf_geomod_authored_identity_capture(level,geometry,&asset,maps,6,rgb,
            2*1024*1024,repeat,&peak));
        CHECK(!memcmp(digests[n],repeat,32));
        if(uids[n]==94) CHECK(!memcmp(digests[n],known94,32));
        for(i=0;i<n;i++) CHECK(memcmp(digests[n],digests[i],32));
        CHECK(asset.windows.face_count==((uids[n]==95 || uids[n]==98)?8:4));
        for(i=0;i<asset.windows.face_count;i++) {
            face_charts[i]=asset.window_origins[i].reference;
            for(j=0;j<manifest.reference_count;j++) if(refs[j].reference==face_charts[i]) break;
            CHECK(j<manifest.reference_count);
        }
        for(i=0;i<manifest.reference_count;i++) {
            CHECK(!refs[i].chart.image.pixels);charts[i]=refs[i].chart;
        }
        publication.mesh=asset.windows;publication.origins=asset.window_origins;
        publication.face_charts=face_charts;publication.materials=materials;
        publication.material_count=manifest.material_count;publication.charts=charts;
        publication.chart_count=manifest.reference_count;
        publication.publication_policy=publication.material_policy=1;
        CHECK(!rf_geomod_publication_digest(&publication,windows[n]));
        for(i=0;i<n;i++) CHECK(memcmp(windows[n],windows[i],32));
        printf("SOURCE_MANIFEST uid%u references%u resident%u peak%u sha",uids[n],
            manifest.reference_count,manifest.resident_bytes,peak);
        for(i=0;i<32;i++)printf("%02x",digests[n][i]);puts("");
        /* Unsupported identity fails before publishing either output. */
        asset.source_uid=79;memset(repeat,0xa5,sizeof(repeat));peak=123;
        CHECK(rf_geomod_authored_identity_capture(level,geometry,&asset,maps,6,rgb,
            2*1024*1024,repeat,&peak)==RF_NOT_FOUND && peak==123);
        for(i=0;i<32;i++)CHECK(repeat[i]==0xa5);
        rf_geomod_authored_post_close(&owner);
    }
    return 0;
}
int main(int argc,char **argv)
{
    static const char *names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
    /* Authored reconstruction policy9; earlier saves have a different identity. */
    static const unsigned char known[32]={0x58,0x93,0x61,0xac,0xbf,0xa8,0x56,0xa9,0x07,0x6d,0xd6,0x12,0x90,0xbb,0xf2,0xd9,
        0x32,0x1e,0x7d,0xbd,0x47,0x18,0xcb,0x0d,0x23,0x12,0xc5,0x86,0xc7,0x0d,0xe6,0x41};
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
    CHECK(!selected_identities(&level,&geometry,maps,&rgb,known));
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_geomod_authored_post_close(&owner);rf_lightmap_rgb_close(&rgb);
    rf_geometry_close(&geometry);rf_vpp_close(&archive);
    puts("PASS installed trusted manifest capture, no pixel borrow, real source windows, budget/capacity/ownership rollback, legacy SHA");return 0;
}
