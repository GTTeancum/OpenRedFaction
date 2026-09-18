/* Actual east courtyard cavity, including unchanged wall/floor geometry. */
#include "rf/geomod_authored_post.h"
#include "rf/authored_identity_capture.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller cavity line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp levels={0},maps[6]={{0}};rf_level level={0};rf_geometry geometry={0};rf_lightmap_rgb_owner rgb={0};
    rf_geomod_authored_post *o=NULL,*old=NULL;rf_geomod_authored_post_view a,b;
    rf_geomod_authored_identity_manifest manifest={0};
    unsigned char digest[32],again[32];uint32_t reference=UINT32_MAX,peak,i,matched=0;
    const char *names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};char path[128];
    float lo[3]={39.136504f,4.128866f,-172.708743f},hi[3]={43.488496f,9.871134f,-161.381987f};
    CHECK(!rf_vpp_open(&levels,"Installed_Game/levelsm.vpp"));CHECK(!rf_level_open(&level,&levels,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_cavity_open_source(&level,&geometry,148,2*1024*1024,&o));
    CHECK(!rf_geomod_authored_post_get(o,&a));
    CHECK(a.source_uid==148 && a.room==0 && a.source.face_count==22 && !a.neighbors.face_count && !a.solid_count);
    for(i=0;i<a.windows.face_count;i++)if(a.window_origins[i].reference==6405)matched++;
    CHECK(matched==1);
    CHECK(!rf_geomod_authored_cavity_admit(o,lo,hi,&reference));CHECK(reference==6405);
    /* The actual cutter may extend below floor4: both retained floor677 and
     * wall6405 must admit their clipped surface patches, with no upward shift. */
    lo[1]=3.934565f;hi[1]=9.676833f;
    CHECK(!rf_geomod_authored_cavity_admit(o,lo,hi,&reference));
    lo[2]=-179;hi[2]=-170;reference=123;
    CHECK(rf_geomod_authored_cavity_admit(o,lo,hi,&reference)==RF_NOT_FOUND && reference==123);
    CHECK(!rf_geomod_authored_cavity_open(&level,&geometry,2*1024*1024,&old));
    CHECK(!rf_geomod_authored_post_get(old,&b));CHECK(b.source_uid==66 && b.room==3 && b.source.face_count==14);
    CHECK(!rf_lightmap_rgb_open(&rgb,&level,16*1024*1024));
    for(i=0;i<6;i++){snprintf(path,sizeof(path),"Installed_Game/%s",names[i]);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!rf_geomod_authored_identity_capture(&level,&geometry,&a,maps,6,&rgb,8*1024*1024,digest,&peak));
    manifest.reference_capacity=a.source.face_count+a.windows.face_count+a.neighbors.face_count;
    manifest.material_capacity=manifest.reference_capacity>128?128:manifest.reference_capacity;
    CHECK(manifest.reference_capacity==130 && manifest.material_capacity==128);
    manifest.materials=calloc(manifest.material_capacity,sizeof(*manifest.materials));
    manifest.references=calloc(manifest.reference_capacity,sizeof(*manifest.references));
    manifest.substrate=calloc(1,sizeof(*manifest.substrate));
    CHECK(manifest.materials && manifest.references && manifest.substrate);
    CHECK(!rf_geomod_authored_identity_capture_manifest(&level,&geometry,&a,maps,6,&rgb,3*1024*1024,again,&peak,&manifest));
    CHECK(peak<=3*1024*1024 && !memcmp(digest,again,32));
    CHECK(manifest.material_count && manifest.reference_count && manifest.substrate->prehashed && !manifest.substrate->image.pixels);
    free(manifest.materials);free(manifest.references);free(manifest.substrate);
    printf("DRILLER_CAVITY source=%u room=%u faces=%u windows=%u resident=%u peak=%u identity_peak=%u\n",a.source_uid,a.room,a.source.face_count,a.windows.face_count,a.resident_bytes,a.peak_bytes,peak);
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_lightmap_rgb_close(&rgb);
    rf_geomod_authored_post_close(&old);rf_geomod_authored_post_close(&o);rf_geometry_close(&geometry);rf_vpp_close(&levels);return 0;
}
