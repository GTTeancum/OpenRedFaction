/* Installed L1S2 source admission only; scene collision/publication remains separate. */
#include "rf/geomod_authored_post.h"
#include <stdio.h>
#define CHECK(call) do { int result=(call); if(result) { fprintf(stderr,"FAIL line%d status%d %s\n",__LINE__,result,#call); return 1; } } while(0)
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};
    rf_geomod_authored_post *owner=NULL;rf_geomod_authored_post_view view;
    if(argc!=2)return 2;
    CHECK(rf_vpp_open(&archive,argv[1]));
    CHECK(rf_level_open(&level,&archive,"L1S2.rfl"));
    CHECK(rf_geometry_open(&geometry,&level,8u*1024u*1024u));
    CHECK(rf_geomod_authored_cavity_open_source(&level,&geometry,8123,2u*1024u*1024u,&owner));
    CHECK(rf_geomod_authored_post_get(owner,&view));
    if(view.source_uid!=8123 || view.room!=8 || view.source.face_count!=48 ||
       view.windows.face_count!=83 || view.replaced_count!=83 || view.solid_count) {
        fprintf(stderr,"FAIL source %u room %u faces %u windows %u replaced %u solids %u\n",
            view.source_uid,view.room,view.source.face_count,view.windows.face_count,
            view.replaced_count,view.solid_count);return 1;
    }
    printf("PASS L1S2 cavity source %u room %u faces %u windows %u resident %u peak %u\n",
        view.source_uid,view.room,view.source.face_count,view.windows.face_count,
        view.resident_bytes,view.peak_bytes);
    rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    return 0;
}
