/* Standalone numeric oracle for actual live RGCH history; no game/window.
 * argv: levelsm.vpp history.rgch [publication.rgp]
 * Optional RGP1: magic, u32 vertices,faces, then rf_geomod_vertex[],
 * rf_geomod_face[] (live compiled-reference source_face), publication_origin[].
 * Renderer material slots deliberately excluded from comparison: asset loader
 * uses compiled texture IDs. UVs/positions/topology/provenance stay exact.
 */
#include "rf/geomod_authored_post.h"
#include "rf/collision_composition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static rf_geomod_publication_work work;
static rf_geomod_vertex vertices[4096],live_vertices[4096];
static rf_geomod_face polygons[768],live_polygons[768];
static rf_geomod_publication_origin origins[768],live_origins[768];
static rf_collision_face collision[768];
static rf_collision_face_filter filters[768];
static float positions[4096][3];
static uint32_t metadata[768];
static int ray(const rf_collision_composition_view *v,const float p[3],const float d[3],
    uint32_t flags,uint32_t *yes,rf_collision_tree_hit *hit,uint32_t *id)
{
    const rf_collision_tree *t=v->tree;
    int s=rf_collision_thin_tree(t->nodes,t->node_count,t->faces,t->face_count,flags,p,d,1,
        t->stack,t->node_capacity,hit,yes);
    if(!s && *yes)*id=v->face_ids[t->source_indices[hit->face_index]];
    return s;
}
static int same_face(const rf_collision_face *a,const rf_collision_face *b)
{
    return a->count==b->count && a->triangle_surface==b->triangle_surface &&
        !memcmp(a->plane,b->plane,40) && !memcmp(&a->filter,&b->filter,sizeof(a->filter)) &&
        !memcmp(a->vertices,b->vertices,a->count*12u);
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geometry_collision_room room={0};
    rf_geomod_authored_post *asset=NULL;rf_geomod_authored_post_view a;
    rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view t;rf_geomod_mesh_view output;
    rf_geomod_publication_job job={0};rf_geomod_publication_cut cuts[RF_GEOMOD_CUT_LIMIT];
    rf_collision_composition *owner=NULL;rf_collision_composition_view before,after;
    rf_collision_face_filter generated;unsigned char history[12380];FILE *file;
    uint32_t n,i,j,k,retained=0,liquids=0,yes,id,oldids[2],floor_patches=0;
    rf_collision_tree_hit hit,oldhits[2];
    float controls[2][3]={{-4.645f,-.9f,2.5f},{-13.7433157f,-1.8f,10.6686643f}},down[3]={0,-1,0};
    float corridor[3]={-4.5f,-.9f,2.5f},across[3]={-1,0,0};
    CHECK(argc==3 || argc==4);file=fopen(argv[2],"rb");CHECK(file);
    n=(uint32_t)fread(history,1,sizeof(history),file);CHECK(n>=28 && fgetc(file)==EOF);fclose(file);
    CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_post_open(&level,&geometry,2*1024*1024,&asset));
    CHECK(!rf_geomod_authored_post_get(asset,&a));
    CHECK(!rf_geometry_collision_room_open(&geometry,a.room,4*1024*1024,&room));
    CHECK(room.tree.face_count==790 && a.replaced_count==4);
    for(i=0;i<4;i++)CHECK(a.replaced_ids[i]==149+i);
    generated=a.source_filters[0];generated.query_flags=0;generated.face_flags=256;
    CHECK(!rf_geomod_terrain_open(&a.source,a.source_filters,&generated,0,4096,800,1024*1024,&terrain));
    {uint32_t width,height;memcpy(&width,history+20,4);memcpy(&height,history+24,4);
     CHECK(width==256 && height==256);CHECK(!rf_geomod_terrain_set_mapping(terrain,width,height));}
    CHECK(!rf_geomod_terrain_history_decode(terrain,history,n));CHECK(!rf_geomod_terrain_get(terrain,&t));
    CHECK(t.cuts>0 && t.cuts<=RF_GEOMOD_CUT_LIMIT);
    for(i=0;i<t.cuts;i++)CHECK(!rf_geomod_terrain_cutter_get(terrain,i,&cuts[i].mesh,cuts[i].kernel,&cuts[i].star));
    job.terrain=t.mesh;job.windows=a.windows;job.neighbors=a.neighbors;
    job.window_origins=a.window_origins;job.neighbor_origins=a.neighbor_origins;
    job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,a.source_uid,UINT32_MAX,149};
    job.source_planes=a.source_planes;job.source_plane_count=a.source.face_count;job.solids=a.solids;
    job.solid_count=a.solid_count;job.cuts=cuts;job.cut_count=t.cuts;
    CHECK(!rf_geomod_publication_build(&job,&work,vertices,4096,polygons,768,origins,&output));
    for(i=0;i<output.face_count;i++) {
        const rf_geomod_publication_origin *o=origins+i;
        CHECK(o->reference!=UINT32_MAX);metadata[i]=o->reference;
        CHECK(!rf_geometry_initial_collision_filter(&geometry,metadata[i],0,filters+i));
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER){CHECK(o->source_face==UINT32_MAX);filters[i].face_flags=256;}
        else if(o->owner==94)CHECK(o->source_face>=550 && o->source_face<=553 && o->reference>=149 && o->reference<=152);
        /* Hidden caps548/549 must never be resurrected as retained source. */
        else CHECK(o->owner==71 || o->owner==95 || o->owner==70);
        floor_patches+=o->owner==71 && o->source_face==415;
        printf("SURFACE %u kind%u owner%u source%u reference%u corners%u\n",i,o->kind,o->owner,
            o->source_face,o->reference,polygons[i].count);
    }
    CHECK(!rf_geomod_collision_faces(&output,filters,positions,4096,collision,768));
    CHECK(!rf_collision_composition_open(&room.tree,room.tree.source_indices,a.replaced_ids,4,
        790+768,2*1024*1024,NULL,&owner));CHECK(!rf_collision_composition_get(owner,&before));
    for(i=0;i<2;i++)CHECK(!ray(&before,controls[i],down,i?0x1004:4,&yes,oldhits+i,oldids+i) && yes);
    CHECK(!rf_collision_composition_prepare(owner,collision,metadata,output.face_count));
    CHECK(!rf_collision_composition_pending(owner,&after));CHECK(after.count==786+output.face_count);
    /* Every untouched source descriptor/filter/vertex survives exactly, including
     * all beam/floor faces, not merely selected successful rays. Source-order
     * IDs are permuted by the tree; match provenance before comparing. */
    for(i=0;i<room.tree.face_count;i++) {
        uint32_t original_id=room.tree.source_indices[i],matches=0;
        if(original_id>=149 && original_id<=152)continue;
        for(j=0;j<after.tree->face_count;j++) {
            uint32_t source=after.tree->source_indices[j];
            if(after.face_ids[source]==original_id && same_face(room.tree.faces+i,after.tree->faces+j))matches++;
        }
        CHECK(matches==1);retained++;
    }
    CHECK(retained==786);
    for(i=0;i<after.tree->face_count;i++)liquids+=!!(after.tree->faces[i].filter.face_flags&4);
    CHECK(liquids==82);
    for(i=0;i<2;i++) {
        CHECK(!ray(&after,controls[i],down,i?0x1004:4,&yes,&hit,&id) && yes && id==oldids[i]);
        CHECK(!memcmp(&hit.hit,&oldhits[i].hit,sizeof(hit.hit)));
        printf("UNCHANGED_CONTROL %s id%u fraction%.9g point%.9g,%.9g,%.9g\n",
            i?"water":"floor",id,hit.hit.fraction,hit.hit.point[0],hit.hit.point[1],hit.hit.point[2]);
    }
    CHECK(!ray(&after,corridor,across,4,&yes,&hit,&id));
    printf("CORRIDOR y%.9g blocked%u face%u fraction%.9g\n",corridor[1],yes,yes?id:UINT32_MAX,yes?hit.hit.fraction:0);
    CHECK(!yes); /* This retained fixture requires the first playable opening. */
    {float under[3]={-5,-.9f,2.5f};
     CHECK(!ray(&after,under,down,4,&yes,&hit,&id) && yes && hit.hit.point[1]==-1.25f);
     CHECK(id==141 || id==142 || id==143 || id==144 || (id>=169 && id<=173));
     printf("EXPOSED_FLOOR id%u height%.9g\n",id,hit.hit.point[1]);}
    corridor[1]=1.5f;CHECK(!ray(&after,corridor,across,4,&yes,&hit,&id));
    printf("UPPER_POST y%.9g blocked%u face%u\n",corridor[1],yes,yes?id:UINT32_MAX);
    if(t.cuts==1)CHECK(yes && id>=149 && id<=152);
    if(t.cuts==2) {
        CHECK(!yes);corridor[1]=1;
        CHECK(!ray(&after,corridor,across,4,&yes,&hit,&id) && !yes);
        puts("SECOND_IMPACT_CORRIDOR y1 blocked0");
    }
    if(argc==4) {
        char magic[4];uint32_t dims[2];file=fopen(argv[3],"rb");CHECK(file);
        CHECK(fread(magic,1,4,file)==4 && !memcmp(magic,"RGP1",4));CHECK(fread(dims,4,2,file)==2);
        CHECK(dims[0]==output.vertex_count && dims[1]==output.face_count);
        CHECK(fread(live_vertices,sizeof(*vertices),dims[0],file)==dims[0]);
        CHECK(fread(live_polygons,sizeof(*polygons),dims[1],file)==dims[1]);
        CHECK(fread(live_origins,sizeof(*origins),dims[1],file)==dims[1] && fgetc(file)==EOF);fclose(file);
        CHECK(!memcmp(vertices,live_vertices,dims[0]*sizeof(*vertices)));
        CHECK(!memcmp(origins,live_origins,dims[1]*sizeof(*origins)));
        for(k=0;k<dims[1];k++) {
            uint32_t source=origins[k].kind==RF_GEOMOD_PUBLICATION_CRATER?UINT32_MAX:origins[k].reference;
            CHECK(polygons[k].first==live_polygons[k].first && polygons[k].count==live_polygons[k].count);
            CHECK(live_polygons[k].source_face==source);
        }
        puts("LIVE_PUBLICATION geometry_uv_provenance_exact1 material_slots_compared0");
    }
    printf("PASS cuts%u publication%u/%u retained%u liquids%u floor_patches%u composed%u\n",
        t.cuts,output.face_count,output.vertex_count,retained,liquids,floor_patches,after.count);
    rf_collision_composition_close(&owner);rf_geomod_terrain_close(&terrain);
    rf_geometry_collision_room_close(&room);rf_geomod_authored_post_close(&asset);
    rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
}
