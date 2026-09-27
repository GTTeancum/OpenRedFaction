/* Installed L1S2 source identity, bounded terrain admission and publication. */
#include "rf/geomod_authored_post.h"
#include "rf/geomod_campaign_room.h"
#include "rf/authored_identity_capture.h"
#include "rf/collision_composition.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(call) do { int result=(call); if(result) { fprintf(stderr,"FAIL line%d status%d %s\n",__LINE__,result,#call); return 1; } } while(0)
static rf_geomod_publication_work publication_work;
static rf_geomod_vertex published_vertices[RF_GEOMOD_PUBLICATION_VERTICES];
static rf_geomod_face published_faces[RF_GEOMOD_PUBLICATION_FACES];
static rf_geomod_publication_origin published_origins[RF_GEOMOD_PUBLICATION_FACES];
static int collision_probe(const rf_geometry *geometry,const rf_geomod_authored_post_view *asset,
    const rf_geomod_mesh_view *published,const rf_geomod_publication_origin *origins)
{
    rf_geometry_collision_world base={0};rf_geometry_collision_overlay overlay={0};
    rf_collision_composition *composition=NULL;rf_collision_composition_view pending;
    rf_collision_face_filter *filters=NULL;rf_collision_face *faces=NULL;
    float (*positions)[3]=NULL;uint32_t *ids=NULL,i,front=0,back=0,side=0;int status;
    const float start[3]={121.208061f,-2.12633848f,-15.f},delta[3]={0,0,-6.f};
    const float side_start[3]={117.5f,-2.12633848f,-15.f};
    rf_geometry_world_hit before={0},after={0},side_hit={0};
    filters=calloc(published->face_count,sizeof(*filters));faces=calloc(published->face_count,sizeof(*faces));
    positions=calloc(published->vertex_count,sizeof(*positions));ids=calloc(published->face_count,sizeof(*ids));
    if(!filters || !faces || !positions || !ids)return 1;
    CHECK(rf_geometry_collision_world_open(geometry,16u*1024u*1024u,&base));
    CHECK(rf_geometry_collision_world_ray(&base,4,start,delta,1.f,&before,&front));
    for(i=0;i<published->face_count;i++) {
        ids[i]=origins[i].reference;
        CHECK(rf_geometry_initial_collision_filter(geometry,ids[i],0,filters+i));
        if(origins[i].kind==RF_GEOMOD_PUBLICATION_CRATER)filters[i].face_flags=256;
    }
    CHECK(rf_geomod_collision_faces(published,filters,positions,published->vertex_count,
        faces,published->face_count));
    CHECK(rf_collision_composition_open(&base.rooms[asset->room].tree,
        base.rooms[asset->room].tree.source_indices,asset->replaced_ids,asset->replaced_count,
        base.rooms[asset->room].tree.face_count+published->face_count,4u*1024u*1024u,NULL,&composition));
    CHECK(rf_collision_composition_prepare(composition,faces,ids,published->face_count));
    CHECK(rf_collision_composition_pending(composition,&pending));
    CHECK(rf_geometry_collision_overlay_open(&base,asset->room,
        base.rooms[asset->room].tree.face_count+published->face_count,128u*1024u,&overlay));
    CHECK(rf_geometry_collision_overlay_bind(&overlay,pending.tree,pending.face_ids,pending.count));
    CHECK(rf_collision_composition_commit(composition));
    CHECK(rf_geometry_collision_world_ray(&overlay.world,4,start,delta,1.f,&after,&back));
    CHECK(rf_geometry_collision_world_ray(&overlay.world,4,side_start,delta,1.f,&side_hit,&side));
    printf("L1S2 collision cut before %u face %u z %.6f after %u face %u z %.6f; adjacent wall %u face %u z %.6f\n",
        front,before.face,before.hit.point[2],back,after.face,after.hit.point[2],side,side_hit.face,side_hit.hit.point[2]);
    status=front && before.face==768 && back && after.hit.point[2]<-17.01f && side && side_hit.face==768?0:1;
    rf_geometry_collision_overlay_close(&overlay);rf_collision_composition_close(&composition);
    rf_geometry_collision_world_close(&base);free(filters);free(faces);free(positions);free(ids);
    return status;
}
static int paired_collision_probe(const rf_geometry *geometry,
    const rf_geomod_authored_post_view *first,const rf_geomod_authored_post_view *second,
    const rf_geomod_mesh_view *published,const rf_geomod_publication_origin *origins)
{
    rf_geometry_collision_world base={0};rf_geometry_collision_overlay overlay={0};
    rf_collision_composition *composition=NULL;rf_collision_composition_view pending;
    rf_collision_face_filter *filters=NULL;rf_collision_face *faces=NULL;
    float (*positions)[3]=NULL;uint32_t *ids=NULL,replaced[256],i,hit_before=0,hit_after=0,hit_inside=0;
    rf_geometry_world_hit before={0},after={0},inside={0};int status;
    const float start[3]={128.f,-1.95f,-17.2f},delta[3]={-3.5f,0,0};
    const float inside_start[3]={125.8f,-1.95f,-17.2f},inside_delta[3]={3.5f,0,0};
    if(first->room!=second->room || first->replaced_count+second->replaced_count>256)return 1;
    memcpy(replaced,first->replaced_ids,first->replaced_count*sizeof(*replaced));
    memcpy(replaced+first->replaced_count,second->replaced_ids,second->replaced_count*sizeof(*replaced));
    filters=calloc(published->face_count,sizeof(*filters));faces=calloc(published->face_count,sizeof(*faces));
    positions=calloc(published->vertex_count,sizeof(*positions));ids=calloc(published->face_count,sizeof(*ids));
    if(!filters || !faces || !positions || !ids)return 1;
    CHECK(rf_geometry_collision_world_open(geometry,16u*1024u*1024u,&base));
    CHECK(rf_geometry_collision_world_ray(&base,4,start,delta,1.f,&before,&hit_before));
    for(i=0;i<published->face_count;i++) {
        ids[i]=origins[i].reference;
        CHECK(rf_geometry_initial_collision_filter(geometry,ids[i],0,filters+i));
        if(origins[i].kind==RF_GEOMOD_PUBLICATION_CRATER)filters[i].face_flags=256;
    }
    CHECK(rf_geomod_collision_faces(published,filters,positions,published->vertex_count,
        faces,published->face_count));
    CHECK(rf_collision_composition_open(&base.rooms[first->room].tree,
        base.rooms[first->room].tree.source_indices,replaced,
        first->replaced_count+second->replaced_count,
        base.rooms[first->room].tree.face_count+published->face_count,
        4u*1024u*1024u,NULL,&composition));
    CHECK(rf_collision_composition_prepare(composition,faces,ids,published->face_count));
    CHECK(rf_collision_composition_pending(composition,&pending));
    CHECK(rf_geometry_collision_overlay_open(&base,first->room,
        base.rooms[first->room].tree.face_count+published->face_count,128u*1024u,&overlay));
    CHECK(rf_geometry_collision_overlay_bind(&overlay,pending.tree,pending.face_ids,pending.count));
    CHECK(rf_collision_composition_commit(composition));
    CHECK(rf_geometry_collision_world_ray(&overlay.world,4,start,delta,1.f,&after,&hit_after));
    CHECK(rf_geometry_collision_world_ray(&overlay.world,4,inside_start,inside_delta,1.f,&inside,&hit_inside));
    printf("L1S2 paired collision outward before %u face %u x %.6f after %u; inward crater %u face %u x %.6f\n",
        hit_before,before.face,before.hit.point[0],hit_after,hit_inside,inside.face,inside.hit.point[0]);
    status=hit_before && before.face==5780 && !hit_after && hit_inside &&
        inside.hit.point[0]>before.hit.point[0]+0.01f?0:1;
    rf_geometry_collision_overlay_close(&overlay);rf_collision_composition_close(&composition);
    rf_geometry_collision_world_close(&base);free(filters);free(faces);free(positions);free(ids);
    return status;
}
static int detail_room_clip_probe(const rf_geometry *geometry,
    const rf_geomod_publication_cut *cut)
{
    static rf_geomod_vertex source_vertices[512];
    static rf_geomod_face source_faces[64];
    static rf_geomod_publication_origin source_origins[64];
    static rf_collision_face_filter filters[RF_GEOMOD_PUBLICATION_FACES];
    static rf_collision_face bound[RF_GEOMOD_PUBLICATION_FACES];
    static float positions[RF_GEOMOD_PUBLICATION_VERTICES][3];
    static uint32_t ids[RF_GEOMOD_PUBLICATION_FACES];
    rf_geometry_collision_world world={0};rf_collision_composition *composition=NULL;
    rf_collision_composition_view pending;rf_collision_tree_hit before={0},after={0};
    uint32_t replaced[64],found_before=0,found_after=0;
    const float ray_start[3]={126.4099f,-1.5967f,-17.7522f};
    const float ray_delta[3]={0.4371f,-0.2428f,0};
    rf_geomod_mesh_view source,clipped;uint32_t i,nf,changed=0;
    if(!geometry || !cut)return 1;
    CHECK(rf_geomod_campaign_room_import(geometry,121,source_vertices,512,
        source_faces,64,replaced,&source));
    nf=source.face_count;
    if(nf!=30 || source.vertex_count!=120)return 1;
    for(i=0;i<nf;i++) {
        rf_geometry_face face;
        CHECK(rf_geometry_get_face(geometry,replaced[i],&face));
        if(face.flags!=8 || face.source_word!=source_faces[i].source_face)return 1;
        source_origins[i]=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_RETAINED,
            9996,face.source_word,replaced[i]};
    }
    CHECK(rf_geomod_publication_cut_neighbors(&source,source_origins,9996,cut,1,
        &publication_work,published_vertices,RF_GEOMOD_PUBLICATION_VERTICES,
        published_faces,RF_GEOMOD_PUBLICATION_FACES,published_origins,&clipped));
    for(i=0;i<clipped.face_count;i++) {
        uint32_t id=published_origins[i].reference;
        if(id==4972 || id==4984 || id==4985 || id==4998)changed++;
    }
    printf("L1S2 room-121 detail clip source %u/%u output %u/%u touched descendants %u\n",
        source.face_count,source.vertex_count,clipped.face_count,clipped.vertex_count,changed);
    if(clipped.face_count==source.face_count || !changed)return 1;
    CHECK(rf_geometry_collision_world_open(geometry,16u*1024u*1024u,&world));
    for(i=0;i<clipped.face_count;i++) {
        ids[i]=published_origins[i].reference;
        CHECK(rf_geometry_initial_collision_filter(geometry,ids[i],0,filters+i));
    }
    CHECK(rf_geomod_collision_faces(&clipped,filters,positions,RF_GEOMOD_PUBLICATION_VERTICES,
        bound,RF_GEOMOD_PUBLICATION_FACES));
    CHECK(rf_collision_composition_open(&world.rooms[121].tree,
        world.rooms[121].tree.source_indices,replaced,nf,
        world.rooms[121].tree.face_count+clipped.face_count,
        4u*1024u*1024u,NULL,&composition));
    CHECK(rf_collision_composition_prepare(composition,bound,ids,clipped.face_count));
    CHECK(rf_collision_composition_pending(composition,&pending));
    CHECK(rf_collision_thin_tree(world.rooms[121].tree.nodes,
        world.rooms[121].tree.node_count,world.rooms[121].tree.faces,
        world.rooms[121].tree.face_count,4,ray_start,ray_delta,1.f,
        world.rooms[121].tree.stack,world.rooms[121].tree.node_count,&before,&found_before));
    CHECK(rf_collision_thin_tree(pending.tree->nodes,pending.tree->node_count,
        pending.tree->faces,pending.tree->face_count,4,ray_start,ray_delta,1.f,
        pending.tree->stack,pending.tree->node_count,&after,&found_after));
    printf("L1S2 detail collision ray before %u face %u after %u face %u\n",
        found_before,found_before?world.rooms[121].tree.source_indices[before.face_index]:UINT32_MAX,
        found_after,found_after?pending.face_ids[pending.tree->source_indices[after.face_index]]:UINT32_MAX);
    changed=found_before && world.rooms[121].tree.source_indices[before.face_index]==4972 &&
        (!found_after || pending.face_ids[pending.tree->source_indices[after.face_index]]!=4972);
    rf_collision_composition_close(&composition);rf_geometry_collision_world_close(&world);
    return changed?0:1;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};
    rf_vpp maps[6]={{0}};rf_lightmap_rgb_owner rgb={0};
    rf_geomod_authored_post *owner=NULL,*neighbor_owner=NULL;
    rf_geomod_authored_post_view view,neighbor_view;
    rf_geomod_authored_identity_manifest manifest={0},neighbor_manifest={0};
    unsigned char identity[32],neighbor_identity[32];uint32_t identity_peak=0,neighbor_identity_peak=0;
    rf_geomod_terrain *terrain=NULL,*neighbor_terrain=NULL;
    rf_geomod_terrain_view terrain_view,neighbor_terrain_view;
    rf_collision_face_filter generated;rf_geomod_template shape;
    rf_geomod_publication_job job={0};rf_geomod_publication_cut cut={0};rf_geomod_mesh_view published;
    const float center[3]={121.208061f,-2.12633848f,-17.f};
    const float basis[9]={1,0,0,0,1,0,0,0,1};int cut_status;uint32_t i,crater_faces=0,retained_faces=0;
    const char *map_names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
    char map_path[512];
    if(argc!=4)return 2;
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
    CHECK(rf_geomod_authored_cavity_open_source(&level,&geometry,8219,2u*1024u*1024u,&neighbor_owner));
    CHECK(rf_geomod_authored_post_get(neighbor_owner,&neighbor_view));
    if(neighbor_view.source_uid!=8219 || neighbor_view.room!=8 ||
       neighbor_view.source.face_count!=14 || neighbor_view.windows.face_count!=8 ||
       neighbor_view.replaced_count!=8 || neighbor_view.solid_count) {
        fprintf(stderr,"FAIL neighbor source %u room %u faces %u windows %u replaced %u solids %u\n",
            neighbor_view.source_uid,neighbor_view.room,neighbor_view.source.face_count,
            neighbor_view.windows.face_count,neighbor_view.replaced_count,neighbor_view.solid_count);
        return 1;
    }
    CHECK(rf_lightmap_rgb_open(&rgb,&level,16u*1024u*1024u));
    for(i=0;i<6;i++) {
        snprintf(map_path,sizeof(map_path),"%s/%s",argv[3],map_names[i]);
        CHECK(rf_vpp_open(maps+i,map_path));
    }
    manifest.reference_capacity=view.source.face_count+view.windows.face_count+view.neighbors.face_count;
    manifest.material_capacity=manifest.reference_capacity>128?128:manifest.reference_capacity;
    manifest.materials=calloc(manifest.material_capacity,sizeof(*manifest.materials));
    manifest.references=calloc(manifest.reference_capacity,sizeof(*manifest.references));
    manifest.substrate=calloc(1,sizeof(*manifest.substrate));
    if(!manifest.materials || !manifest.references || !manifest.substrate)return 1;
    CHECK(rf_geomod_authored_identity_capture_manifest(&level,&geometry,&view,maps,6,&rgb,
        3u*1024u*1024u,identity,&identity_peak,&manifest));
    if(!manifest.material_count || !manifest.reference_count || identity_peak>3u*1024u*1024u)return 1;
    neighbor_manifest.reference_capacity=neighbor_view.source.face_count+neighbor_view.windows.face_count;
    neighbor_manifest.material_capacity=neighbor_manifest.reference_capacity;
    neighbor_manifest.materials=calloc(neighbor_manifest.material_capacity,sizeof(*neighbor_manifest.materials));
    neighbor_manifest.references=calloc(neighbor_manifest.reference_capacity,sizeof(*neighbor_manifest.references));
    neighbor_manifest.substrate=calloc(1,sizeof(*neighbor_manifest.substrate));
    if(!neighbor_manifest.materials || !neighbor_manifest.references || !neighbor_manifest.substrate)return 1;
    CHECK(rf_geomod_authored_identity_capture_manifest(&level,&geometry,&neighbor_view,maps,6,&rgb,
        3u*1024u*1024u,neighbor_identity,&neighbor_identity_peak,&neighbor_manifest));
    if(!neighbor_manifest.material_count || !neighbor_manifest.reference_count ||
       neighbor_identity_peak>3u*1024u*1024u || !memcmp(identity,neighbor_identity,32))return 1;
    generated=view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
    CHECK(rf_geomod_terrain_open(&view.source,view.source_filters,&generated,1,
        4096,1024,1152u*1024u,&terrain));
    CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
    if(terrain_view.mesh.face_count!=48 || terrain_view.cuts!=0) {
        fprintf(stderr,"FAIL terrain faces %u cuts %u\n",terrain_view.mesh.face_count,terrain_view.cuts);
        return 1;
    }
    CHECK(rf_geomod_template_load(argv[2],&shape));
    {
        float minimum[3],maximum[3];uint32_t reference=UINT32_MAX;
        CHECK(rf_geomod_template_bounds(&shape,center,basis,1.f,NULL,0,minimum,maximum));
        cut_status=rf_geomod_authored_cavity_admit(owner,minimum,maximum,&reference);
        if(cut_status || reference!=768) {
            fprintf(stderr,"FAIL cutter admission status %d reference %u\n",cut_status,reference);
            return 1;
        }
    }
    {
        /* The live second bit reaches UID8219 room-8 geometry and UID9996
         * room-121 detail. The single-owner admission must still reject it. */
        const float minimum[3]={121.107674f,-2.405448f,-19.962753f};
        const float maximum[3]={126.905739f,0.702065f,-13.505847f};
        uint32_t reference=UINT32_MAX;
        cut_status=rf_geomod_authored_cavity_admit(owner,minimum,maximum,&reference);
        if(cut_status!=RF_NOT_FOUND) {
            fprintf(stderr,"FAIL adjacent brush admission status %d\n",cut_status);
            return 1;
        }
    }
    CHECK(rf_geomod_terrain_cut_template(terrain,&shape,center,basis,1.f,0));
    CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
    if(terrain_view.cuts!=1 || terrain_view.mesh.face_count<=48 ||
       terrain_view.resident_bytes>1152u*1024u || terrain_view.peak_bytes>1152u*1024u) {
        fprintf(stderr,"FAIL cut faces %u cuts %u resident %u peak %u\n",
            terrain_view.mesh.face_count,terrain_view.cuts,
            terrain_view.resident_bytes,terrain_view.peak_bytes);return 1;
    }
    CHECK(rf_geomod_terrain_cutter_get(terrain,0,&cut.mesh,cut.kernel,&cut.star));
    job.terrain=terrain_view.mesh;job.windows=view.windows;job.neighbors=view.neighbors;
    job.window_origins=view.window_origins;job.neighbor_origins=view.neighbor_origins;
    job.source_planes=view.source_planes;job.source_plane_count=view.source.face_count;
    job.cuts=&cut;job.cut_count=1;
    job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,8123,UINT32_MAX,768};
    cut_status=rf_geomod_publication_build_cavity(&job,&publication_work,
        published_vertices,RF_GEOMOD_PUBLICATION_VERTICES,
        published_faces,RF_GEOMOD_PUBLICATION_FACES,published_origins,&published);
    CHECK(cut_status);
    for(i=0;i<published.face_count;i++) {
        crater_faces+=published_origins[i].kind==RF_GEOMOD_PUBLICATION_CRATER;
        retained_faces+=published_origins[i].kind==RF_GEOMOD_PUBLICATION_RETAINED;
    }
    if(!crater_faces || !retained_faces || published.face_count<view.windows.face_count) {
        fprintf(stderr,"FAIL publication faces %u crater %u retained %u\n",
            published.face_count,crater_faces,retained_faces);return 1;
    }
    if(collision_probe(&geometry,&view,&published,published_origins))return 1;
    printf("L1S2 cut core faces %u publication faces %u crater %u retained %u resident %u peak %u\n",
        terrain_view.mesh.face_count,published.face_count,crater_faces,retained_faces,
        terrain_view.resident_bytes,terrain_view.peak_bytes);
    {
        const float next_center[3]={124.314514f,-2.12633848f,-17.f};
        const float next_basis[9]={-0.287775129f,1.59786077e-06f,-0.957698166f,
            -0.00494431565f,0.999986291f,0.0014873792f,
            0.957684517f,0.00516320998f,-0.287771463f};
        rf_geomod_shallow_limit shallow={{0,-1,0},0.4f};
        rf_geomod_publication_cut first_cuts[2],second_cut;
        rf_geomod_publication_job grouped[2];uint32_t owner_faces[2]={0,0};
        generated=neighbor_view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
        CHECK(rf_geomod_terrain_open(&neighbor_view.source,neighbor_view.source_filters,&generated,1,
            4096,1024,1152u*1024u,&neighbor_terrain));
        CHECK(rf_geomod_terrain_cut_template_limits(terrain,&shape,next_center,next_basis,1.f,0,&shallow,1));
        CHECK(rf_geomod_terrain_cut_template_limits(neighbor_terrain,&shape,next_center,next_basis,1.f,0,&shallow,1));
        CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
        CHECK(rf_geomod_terrain_get(neighbor_terrain,&neighbor_terrain_view));
        for(i=0;i<2;i++)CHECK(rf_geomod_terrain_cutter_get(terrain,i,
            &first_cuts[i].mesh,first_cuts[i].kernel,&first_cuts[i].star));
        CHECK(rf_geomod_terrain_cutter_get(neighbor_terrain,0,
            &second_cut.mesh,second_cut.kernel,&second_cut.star));
        grouped[0]=job;grouped[0].terrain=terrain_view.mesh;
        grouped[0].cuts=first_cuts;grouped[0].cut_count=2;
        grouped[1]=(rf_geomod_publication_job){0};
        grouped[1].terrain=neighbor_terrain_view.mesh;
        grouped[1].windows=neighbor_view.windows;
        grouped[1].window_origins=neighbor_view.window_origins;
        grouped[1].source_planes=neighbor_view.source_planes;
        grouped[1].source_plane_count=neighbor_view.source.face_count;
        grouped[1].cuts=&second_cut;grouped[1].cut_count=1;
        grouped[1].crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,
            8219,UINT32_MAX,neighbor_view.replaced_ids[0]};
        CHECK(rf_geomod_publication_build_cavity_groups(grouped,2,2,&publication_work,
            published_vertices,RF_GEOMOD_PUBLICATION_VERTICES,
            published_faces,RF_GEOMOD_PUBLICATION_FACES,published_origins,&published));
        for(i=0;i<published.face_count;i++) {
            if(published_origins[i].owner==8123)owner_faces[0]++;
            if(published_origins[i].owner==8219)owner_faces[1]++;
        }
        if(!owner_faces[0] || !owner_faces[1])return 1;
        printf("L1S2 paired second cut source faces %u/%u published faces %u owners %u/%u\n",
            terrain_view.mesh.face_count,neighbor_terrain_view.mesh.face_count,
            published.face_count,owner_faces[0],owner_faces[1]);
        if(paired_collision_probe(&geometry,&view,&neighbor_view,&published,published_origins))return 1;
        if(detail_room_clip_probe(&geometry,&second_cut))return 1;
    }
    CHECK(rf_geomod_terrain_reset(terrain));CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
    if(terrain_view.cuts || terrain_view.mesh.face_count!=48)return 1;
    printf("PASS L1S2 cavity source %u room %u faces %u windows %u resident %u peak %u\n",
        view.source_uid,view.room,view.source.face_count,view.windows.face_count,
        view.resident_bytes,view.peak_bytes);
    printf("L1S2 identity materials %u references %u peak %u\n",
        manifest.material_count,manifest.reference_count,identity_peak);
    printf("L1S2 neighbor identity materials %u references %u peak %u\n",
        neighbor_manifest.material_count,neighbor_manifest.reference_count,neighbor_identity_peak);
    free(neighbor_manifest.materials);free(neighbor_manifest.references);free(neighbor_manifest.substrate);
    free(manifest.materials);free(manifest.references);free(manifest.substrate);
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_lightmap_rgb_close(&rgb);
    rf_geomod_terrain_close(&neighbor_terrain);rf_geomod_terrain_close(&terrain);
    rf_geomod_authored_post_close(&neighbor_owner);
    rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    return 0;
}
