#include "rf/collision.h"
#include "rf/geometry.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    struct {float lo[3],hi[3],start[3],end[3],point[3];} input;
    struct {int32_t status;uint32_t hit;float point[3];} output;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--group-position")) {
        struct {rf_group_translation_step step;rf_group_translation_progress progress;float position[3];} in;
        struct {int32_t status;uint32_t arrival;float pending[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_group_translation_position(&in.step,&in.progress,in.position,out.pending,&out.arrival);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-integrate")) {
        rf_group_translation_step in;
        struct {int32_t status;rf_group_translation_progress result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_group_translation_integrate(&in,&out.result);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-arrive")) {
        struct {uint32_t count;rf_group_motion_state state;} in;
        struct {int32_t status;uint32_t sounds;rf_group_motion_state state;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.sounds=0xa5a5a5a5;
            out.status=rf_group_translation_arrive(&out.state,in.count,&out.sounds);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-activate")) {
        struct {uint32_t count;rf_group_motion_state state;} in;
        struct {int32_t status;rf_group_motion_state state;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.status=rf_group_motion_activate(&out.state,in.count);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--attach-movers")) {
        struct {uint32_t count,controller,flags,mode,refs_count,handles_count,capacity;float rotation;
            rf_group_object objects[16];uint32_t refs[32],handles[32];} in;
        struct {int32_t status;uint32_t refs_count,handles_count;float rotation;
            rf_group_object objects[16];uint32_t refs[32],handles[32];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.refs_count=in.refs_count;out.handles_count=in.handles_count;out.rotation=in.rotation;
            memcpy(out.objects,in.objects,sizeof(out.objects));memcpy(out.refs,in.refs,sizeof(out.refs));memcpy(out.handles,in.handles,sizeof(out.handles));
            out.status=in.count>16 || in.refs_count>32 || in.capacity>32?RF_RANGE:
                rf_group_attach_movers(out.objects,in.count,in.controller,in.flags,in.mode,out.refs,&out.refs_count,out.handles,&out.handles_count,in.capacity,&out.rotation);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-flags")) {
        struct {uint32_t count;uint8_t flags[6],padding[2];float timing[2];} in;
        struct {int32_t status;uint32_t flags;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_level_group group={0};rf_level_group_key key={0};
            group.key_count=in.count;memcpy(group.flags,in.flags,6);memcpy(key.timing+3,in.timing,8);
            memset(&out,0xa5,sizeof(out));out.status=rf_level_group_initial_flags(&group,&key,&out.flags);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && !strcmp(argv[1],"--groups")) {
        rf_vpp archive;rf_level level;rf_level_group_reader reader;rf_level_group g;int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_level_groups_begin(&level,&reader))return 2;
        while((status=rf_level_group_next(&reader,&g))==RF_OK) {
            uint32_t i,j;rf_level_group_key key;
            if(fwrite(&g,sizeof(g),1,stdout)!=1)return 3;
            for(i=0;i<g.key_count;i++) {
                if(rf_level_group_key_at(&level,&g,i,&key))return 4;
                if(fwrite(&key,sizeof(key),1,stdout)!=1)return 3;
            }
            for(i=0;i<2;i++)for(j=0;j<g.ids_count[i];j++) {
                uint32_t uid;if(rf_level_group_id_at(&level,&g,i,j,&uid))return 5;
                if(fwrite(&uid,4,1,stdout)!=1)return 3;
            }
            {rf_level_group guard={0},before;rf_level_group_reader cut=reader,prior;
                cut.cursor=g.offset;cut.index--;cut.section.size=g.offset+g.bytes-1;prior=cut;
                memset(&guard,0xa5,sizeof(guard));before=guard;
                if(rf_level_group_next(&cut,&guard)!=RF_FORMAT || memcmp(&guard,&before,sizeof(guard)) || memcmp(&cut,&prior,sizeof(cut)))return 6;
            }
        }
        rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:7;
    }
    if(argc==4 && !strcmp(argv[1],"--combined-world")) {
        rf_vpp archive;rf_level level;rf_geometry geometry={0};rf_geometry_movers source={0};
        rf_geometry_collision_world world={0};rf_geometry_collision_movers movers={0},empty={0};
        uint32_t *ids,i,j,k,pass,group,queries=0,hits=0,moving_hits=0,static_hits=0,hash[2]={2166136261u,2166136261u},bytes;
        void *poison=NULL;int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8*1024*1024))return 2;
        if(rf_geometry_collision_world_open(&geometry,8*1024*1024,&world))return 3;
        status=rf_geometry_movers_open(&level,8*1024*1024,&source);if(status && status!=RF_NOT_FOUND)return 4;
        ids=(uint32_t *)malloc(source.count?source.count*4:4);if(!ids)return 5;
        for(i=0;i<source.count;i++)ids[i]=0x12340000+i;
        if(rf_geometry_collision_movers_open(&source,ids,8*1024*1024,&movers))return 6;
        free(ids);bytes=geometry.bytes+source.allocated_bytes;
        for(pass=0;pass<2;pass++) {
            for(group=0;group<2;group++)for(i=0;i<(group?movers.count:world.room_count);i++) {
                const rf_collision_face *faces=group?movers.owned[i].faces:world.rooms[i].tree.faces;
                uint32_t count=group?movers.owned[i].count:(world.rooms[i].tree.face_count?1:0);
                for(j=0;j<count;j++) {
                    const rf_collision_face *face=faces+j;float start[3],end[3],delta[3];uint32_t v,visible;
                    rf_collision_ray_hit local={0},global;
                    struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
                    const unsigned char *raw=(const unsigned char *)&out;
                    for(v=0;v<face->count;v++)for(k=0;k<3;k++)local.point[k]+=face->vertices[v][k];
                    for(k=0;k<3;k++) {local.point[k]/=face->count;local.normal[k]=face->plane[k];}
                    global=local;
                    if(group && rf_collision_contact_world(&local,movers.views[i].output_origin,movers.views[i].output_matrix,&global))return 7;
                    for(k=0;k<3;k++) {
                        start[k]=global.point[k]+global.normal[k]+.0037f*(k+1);
                        end[k]=global.point[k]-global.normal[k]+.005f*(k+1);delta[k]=end[k]-start[k];
                    }
                    if(!group) {
                        rf_geometry_world_hit reference;rf_collision_solid_hit mapped;uint32_t a,b;
                        if(rf_geometry_collision_world_ray(&world,0x464,start,delta,1,&reference,&a) ||
                            rf_geometry_collision_ray(&world,&empty,start,end,0x26,&mapped,&b) || a!=b)return 8;
                        if(a && (memcmp(&reference.hit,&mapped.hit,28) || reference.face!=mapped.face_index || reference.room!=mapped.room || mapped.solid_index!=UINT32_MAX))return 9;
                    }
                    memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_ray(&world,&movers,start,end,0x26,&out.hit,&out.matched);
                    if(out.status || rf_geometry_collision_ray(&world,&movers,start,end,0x26,NULL,&visible) || out.matched!=visible)return 10;
                    if(!pass) {queries++;hits+=out.matched;}
                    if(out.matched) {
                        if(out.hit.solid_index!=UINT32_MAX) {
                            if(out.hit.solid_index>=movers.count || out.hit.face_index>=movers.owned[out.hit.solid_index].count ||
                                out.hit.object_id!=movers.views[out.hit.solid_index].object_id || out.hit.room!=UINT32_MAX)return 11;
                            if(!pass)moving_hits++;
                        } else {
                            const rf_collision_tree *tree;uint32_t found=0;
                            if(out.hit.room>=world.room_count || out.hit.object_id!=UINT32_MAX)return 12;
                            tree=&world.rooms[out.hit.room].tree;
                            for(v=0;v<tree->face_count;v++)if(tree->source_indices[v]==out.hit.face_index)found=1;
                            if(!found)return 13;
                            if(!pass)static_hits++;
                        }
                    }
                    for(k=0;k<sizeof(out);k++)hash[pass]=(hash[pass]^raw[k])*16777619u;
                }
            }
            if(!pass) {rf_geometry_close(&geometry);rf_geometry_movers_close(&source);rf_vpp_close(&archive);poison=malloc(bytes);if(!poison)return 14;memset(poison,0xdd,bytes);}
        }
        if(hash[0]!=hash[1])return 15;
        printf("%u %u %u %u %u %u %u %u\n",world.room_count,movers.count,queries,hits,moving_hits,static_hits,hash[0],world.allocated_bytes+movers.allocated_bytes);
        free(poison);rf_geometry_collision_world_close(&world);rf_geometry_collision_movers_close(&movers);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--bound-movers")) {
        rf_vpp archive;rf_level level;rf_geometry_movers source={0};
        rf_geometry_collision_movers owned={0},exact={0},guard;uint32_t *ids,i;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        if(rf_geometry_movers_open(&level,8*1024*1024,&source))return 3;
        ids=(uint32_t *)malloc(source.count?source.count*4:4);if(!ids)return 4;
        for(i=0;i<source.count;i++)ids[i]=0x12340000+i; /* Explicit diagnostic runtime handles. */
        if(rf_geometry_collision_movers_open(&source,ids,8*1024*1024,&owned))return 5;
        if(rf_geometry_collision_movers_open(&source,ids,owned.peak_bytes,&exact))return 6;
        rf_geometry_collision_movers_close(&exact);
        memset(&guard,0xa5,sizeof(guard));exact=guard;
        if(rf_geometry_collision_movers_open(&source,ids,owned.peak_bytes-1,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)))return 7;
        free(ids);rf_geometry_movers_close(&source);rf_vpp_close(&archive);
        if(fwrite(&owned.count,4,1,stdout)!=1 || fwrite(&owned.allocated_bytes,4,1,stdout)!=1 || fwrite(&owned.peak_bytes,4,1,stdout)!=1)return 8;
        for(i=0;i<owned.count;i++) {
            rf_collision_solid_view *view=owned.views+i;
            if(view->flat_faces!=owned.owned[i].faces || view->flat_count!=owned.owned[i].count ||
                view->rooms || view->room_count || view->primary || view->primary_count || view->children || view->child_count)return 9;
            if(fwrite(owned.uids+i,4,1,stdout)!=1 || fwrite(&view->object_id,4,1,stdout)!=1 ||
                fwrite(&view->flat_count,4,1,stdout)!=1 || fwrite(view->minimum,120,1,stdout)!=1)return 8;
        }
        rf_geometry_collision_movers_close(&owned);rf_geometry_collision_movers_close(&owned);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--vertex-bounds")) {
        uint32_t count;float (*vertices)[3];struct {int32_t status;rf_collision_bounds bounds;} out;
        while(fread(&count,4,1,stdin)==1) {
            if(count>65536)return 2;
            vertices=(float(*)[3])malloc(count?count*12:12);if(!vertices)return 3;
            if(fread(vertices,12,count,stdin)!=count)return 4;
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_vertex_bounds(vertices,count,&out.bounds);free(vertices);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 5;
        }
        return ferror(stdin)?6:0;
    }
    if(argc==4 && !strcmp(argv[1],"--mover-queries")) {
        rf_vpp archive;rf_level level;rf_geometry_movers movers={0};
        rf_geometry_collision_flat *owned;uint32_t i,count;
        struct {uint32_t mover,flags;float start[3],delta[3],origin[3],matrix[3][3],radius,limit;} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_tree_hit hit;} out;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        if(rf_geometry_movers_open(&level,8*1024*1024,&movers))return 3;
        count=movers.count;owned=(rf_geometry_collision_flat *)calloc(count?count:1,sizeof(*owned));if(!owned)return 4;
        for(i=0;i<count;i++)if(rf_geometry_collision_flat_open(&movers.items[i].geometry,8*1024*1024,owned+i))return 5;
        rf_geometry_movers_close(&movers);rf_vpp_close(&archive);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));
            out.status=in.mover>=count?RF_RANGE:rf_collision_flat_faces(owned[in.mover].faces,owned[in.mover].count,
                in.flags,in.start,in.delta,in.origin,in.matrix,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 6;
        }
        for(i=0;i<count;i++)rf_geometry_collision_flat_close(owned+i);
        free(owned);return ferror(stdin)?7:0;
    }
    if(argc==4 && !strcmp(argv[1],"--mover-faces")) {
        rf_vpp archive;rf_level level;rf_geometry_movers movers={0};
        rf_geometry_collision_flat *owned;uint32_t i,j,count,peak=0;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        if(rf_geometry_movers_open(&level,8*1024*1024,&movers))return 3;
        count=movers.count;owned=(rf_geometry_collision_flat *)calloc(count?count:1,sizeof(*owned));if(!owned)return 4;
        for(i=0;i<count;i++) {
            rf_geometry_collision_flat exact={0},guard;
            if(rf_geometry_collision_flat_open(&movers.items[i].geometry,8*1024*1024,owned+i))return 5;
            if(owned[i].allocated_bytes>peak)peak=owned[i].allocated_bytes;
            if(rf_geometry_collision_flat_open(&movers.items[i].geometry,owned[i].allocated_bytes,&exact))return 6;
            rf_geometry_collision_flat_close(&exact);
            memset(&guard,0xa5,sizeof(guard));exact=guard;
            if(rf_geometry_collision_flat_open(&movers.items[i].geometry,owned[i].allocated_bytes-1,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)))return 7;
        }
        rf_geometry_movers_close(&movers);rf_vpp_close(&archive);
        if(fwrite(&count,4,1,stdout)!=1 || fwrite(&peak,4,1,stdout)!=1)return 8;
        for(i=0;i<count;i++) {
            if(fwrite(&owned[i].count,4,1,stdout)!=1)return 8;
            for(j=0;j<owned[i].count;j++) {
                rf_collision_face *face=owned[i].faces+j;
                if(fwrite(&j,4,1,stdout)!=1 || fwrite(&face->count,4,1,stdout)!=1 ||
                    fwrite(face->plane,40,1,stdout)!=1 || fwrite(&face->filter,24,1,stdout)!=1 ||
                    fwrite(face->vertices,12,face->count,stdout)!=face->count)return 8;
            }
            rf_geometry_collision_flat_close(owned+i);
        }
        free(owned);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--movers")) {
        rf_vpp archive;rf_level level;rf_geometry_movers m={0},exact={0},guard;
        uint32_t i,budget;int status;
        if(rf_vpp_open(&archive,argv[2]))return 2;
        status=rf_level_open(&level,&archive,argv[3]);
        if(!status)status=rf_geometry_movers_open(&level,8*1024*1024,&m);
        if(status)return 3;
        budget=m.allocated_bytes;
        if(rf_geometry_movers_open(&level,budget,&exact))return 4;
        rf_geometry_movers_close(&exact);
        memset(&guard,0xa5,sizeof(guard));exact=guard;
        if(rf_geometry_movers_open(&level,budget-1,&exact)!=RF_RANGE || memcmp(&exact,&guard,sizeof(exact)))return 5;
        for(i=0;i<m.count;i++) {
            rf_level truncated=level;uint32_t j,k;
            uint32_t cuts[4]={m.items[i].offset,m.items[i].geometry_offset-1,
                m.items[i].geometry_offset+m.items[i].geometry.bytes-1,
                m.items[i].offset+m.items[i].bytes-1};
            for(j=0;j<4;j++) {
                for(k=0;k<truncated.section_count;k++)if(truncated.sections[k].type==0x2000)truncated.sections[k].size=cuts[j];
                exact=guard;
                if(rf_geometry_movers_open(&truncated,8*1024*1024,&exact)!=RF_FORMAT || memcmp(&exact,&guard,sizeof(exact)))return 8;
            }
        }
        rf_vpp_close(&archive); /* All geometry access below must be owned. */
        printf("%u %u\n",m.count,budget);
        for(i=0;i<m.count;i++) {
            rf_geometry_mover *item=m.items+i;rf_geometry *g=&item->geometry;uint32_t j;
            printf("%d %u %u %u %u %u %u %u %u %u",item->uid,item->offset,item->bytes,item->geometry_offset,g->bytes,g->textures,g->rooms,g->vertices,g->faces,g->mappings);
            for(j=0;j<3;j++)printf(" %u",item->trailer[j]);
            for(j=0;j<3;j++)printf(" %.9g",item->position[j]);
            for(j=0;j<9;j++)printf(" %.9g",item->orientation[j/3][j%3]);
            printf("\n");
            for(j=0;j<g->faces;j++) {
                rf_geometry_face face;uint32_t k;
                if(rf_geometry_get_face(g,j,&face))return 6;
                for(k=0;k<face.corners;k++) {
                    rf_geometry_corner corner;float vertex[3];
                    if(rf_geometry_get_corner(g,j,k,&corner) || rf_geometry_vertex(g,corner.vertex,vertex))return 7;
                }
            }
        }
        rf_geometry_movers_close(&m);rf_geometry_movers_close(&m);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--flat-query")) {
        struct {float z[4];uint32_t count,flags;float start[3],delta[3],radius,limit,origin[3],matrix[3][3];} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_tree_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[4]={0};float vertices[4][4][3];uint32_t i,j;
            for(i=0;i<4;i++) {
                faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<3;j++) {faces[i].minimum[j]=j==2?in.z[i]-.0001f:-2.0001f;faces[i].maximum[j]=j==2?in.z[i]+.0001f:2.0001f;}
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=in.z[i];}
            }
            memset(&out,0xa5,sizeof(out));out.status=in.count>4?RF_RANGE:rf_collision_flat_faces(faces,in.count,in.flags,in.start,in.delta,in.origin,in.matrix,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--solid-ray-posed")) {
        struct {float z[3];uint32_t enabled[3],flags;float start[3],end[3],poses[2][30];uint32_t want_result;} in;
        struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_solid_view solids[3]={0};rf_collision_room_view rooms[3]={0};rf_collision_tree trees[3]={0};
            rf_collision_node nodes[3]={0};rf_collision_face faces[3]={0};float vertices[3][4][3];uint32_t primary=0,work[3],i,j;
            for(i=0;i<3;i++) {
                float z=in.z[i];rf_collision_solid_view *solid=solids+i;
                solid->rooms=rooms+i;solid->room_count=1;solid->primary=&primary;solid->primary_count=1;solid->object_id=100+i;
                solid->input_matrix[0][0]=solid->input_matrix[1][1]=solid->input_matrix[2][2]=1;
                memcpy(solid->output_matrix,solid->input_matrix,36);
                for(j=0;j<3;j++) {
                    solid->minimum[j]=rooms[i].minimum[j]=nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-3;
                    solid->maximum[j]=rooms[i].maximum[j]=nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:3;
                }
                rooms[i].tree=trees+i;trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=work+i;
                nodes[i].face_count=in.enabled[i];nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-3:3;vertices[i][j][1]=j<2?-3:3;vertices[i][j][2]=z;}
            }
            for(i=0;i<2;i++)memcpy(solids[i].minimum,in.poses[i],120);
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_ray_solids(solids,2,solids+2,in.start,in.end,in.flags,in.want_result?&out.hit:NULL,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--solid-ray-flat")) {
        struct {float z[3];uint32_t enabled[3],flags;float start[3],end[3];} in;
        struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_solid_view solids[3]={0};rf_collision_room_view rooms[3]={0};rf_collision_tree trees[3]={0};
            rf_collision_node nodes[3]={0};rf_collision_face faces[3]={0};float vertices[3][4][3];uint32_t primary=0,work[3],i,j;
            for(i=0;i<3;i++) {
                float z=in.z[i];rf_collision_solid_view *solid=solids+i;
                solid->rooms=rooms+i;solid->room_count=1;solid->primary=&primary;solid->primary_count=1;solid->object_id=100+i;
                solid->input_matrix[0][0]=solid->input_matrix[1][1]=solid->input_matrix[2][2]=1;
                memcpy(solid->output_matrix,solid->input_matrix,36);
                for(j=0;j<3;j++) {
                    solid->minimum[j]=rooms[i].minimum[j]=nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-3;
                    solid->maximum[j]=rooms[i].maximum[j]=nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:3;
                }
                rooms[i].tree=trees+i;trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=work+i;
                nodes[i].face_count=in.enabled[i];nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-3:3;vertices[i][j][1]=j<2?-3:3;vertices[i][j][2]=z;}
            }
            for(i=0;i<3;i++) {solids[i].room_count=0;solids[i].flat_faces=faces+i;solids[i].flat_count=in.enabled[i];}
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_ray_solids(solids,2,solids+2,in.start,in.end,in.flags,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--solid-ray")) {
        struct {float z[3];uint32_t enabled[3],flags;float start[3],end[3];} in;
        struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_solid_view solids[3]={0};rf_collision_room_view rooms[3]={0};rf_collision_tree trees[3]={0};
            rf_collision_node nodes[3]={0};rf_collision_face faces[3]={0};float vertices[3][4][3];uint32_t primary=0,work[3],i,j;
            for(i=0;i<3;i++) {
                float z=in.z[i];rf_collision_solid_view *solid=solids+i;
                solid->rooms=rooms+i;solid->room_count=1;solid->primary=&primary;solid->primary_count=1;solid->object_id=100+i;
                solid->input_matrix[0][0]=solid->input_matrix[1][1]=solid->input_matrix[2][2]=1;
                memcpy(solid->output_matrix,solid->input_matrix,36);
                for(j=0;j<3;j++) {
                    solid->minimum[j]=rooms[i].minimum[j]=nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-3;
                    solid->maximum[j]=rooms[i].maximum[j]=nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:3;
                }
                rooms[i].tree=trees+i;trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=work+i;
                nodes[i].face_count=in.enabled[i];nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-3:3;vertices[i][j][1]=j<2?-3:3;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_ray_solids(solids,2,solids+2,in.start,in.end,in.flags,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--contact-world")) {
        struct {rf_collision_ray_hit local;float origin[3],matrix[3][3];} in;
        struct {int32_t status;rf_collision_ray_hit world;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_contact_world(&in.local,in.origin,in.matrix,&out.world);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--query-local")) {
        struct {float start[3],delta[3],origin[3],matrix[3][3];uint32_t flags;} in;
        struct {int32_t status;uint32_t active;float start[3],delta[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_query_local(in.start,in.delta,in.origin,in.matrix,in.flags,out.start,out.delta,&out.active);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-edge")) {
        struct {float start[3],delta[3],radius,a[3],b[3],limit;} in;
        struct {int32_t status;uint32_t hit;float fraction,point[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_sphere_edge(in.start,in.delta,in.radius,in.a,in.b,in.limit,&out.fraction,out.point,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-plane")) {
        struct {float start[3],delta[3],radius,plane[4];} in;
        struct {int32_t status;uint32_t hit;float fraction,point[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_sphere_plane(in.start,in.delta,in.radius,in.plane,&out.fraction,out.point,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && !strcmp(argv[1],"--world")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0},guard,sentinel;
        uint32_t i,j,k,pass,faces=0,queries=0,hits=0,errors=0,hashes[2]={2166136261u,2166136261u},geometry_bytes;void *poison=NULL;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        memset(&guard,0xa5,sizeof(guard));sentinel=guard;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes,&guard))return 6;
        rf_geometry_collision_world_close(&guard);
        for(i=0;i<world.room_count;i++) {
            const rf_collision_tree *tree=&world.rooms[i].tree;
            if(world.views[i].tree!=tree || memcmp(world.views[i].minimum,world.rooms[i].minimum,24))return 7;
            faces+=tree->face_count;
            for(j=0;j<tree->face_count;j++) {rf_geometry_face face;if(rf_geometry_get_face(&geometry,tree->source_indices[j],&face) || face.room!=i)return 8;}
        }
        if(faces!=geometry.faces)return 9;
        geometry_bytes=geometry.bytes;
        for(pass=0;pass<2;pass++) {
            for(i=0;i<world.room_count;i++) {
                const rf_collision_tree *tree=&world.rooms[i].tree;const rf_collision_face *face;float start[3]={0},delta[3];
                struct {int32_t status;uint32_t matched;rf_geometry_world_hit hit;} out;const unsigned char *bytes=(const unsigned char*)&out;
                if(!tree->face_count)continue;face=tree->faces;
                for(j=0;j<face->count;j++)for(k=0;k<3;k++)start[k]+=face->vertices[j][k];
                for(k=0;k<3;k++) {start[k]=start[k]/face->count+face->plane[k]+.0037f*(k+1);delta[k]=-2*face->plane[k]+.0013f*(k+1);}
                memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_world_ray(&world,0x460,start,delta,1,&out.hit,&out.matched);
                if(!pass) {queries++;errors+=out.status!=0;if(!out.status)hits+=out.matched;}
                if(!out.status && out.matched && (out.hit.face>=faces || out.hit.room>=world.room_count))return 10;
                for(j=0;j<sizeof(out);j++)hashes[pass]=(hashes[pass]^bytes[j])*16777619u;
            }
            if(!pass) {rf_geometry_close(&geometry);poison=malloc(geometry_bytes);if(!poison)return 11;memset(poison,0xdd,geometry_bytes);}
        }
        if(hashes[0]!=hashes[1])return 12;
        printf("%u %u %u %u %u %u %u %u %u %u\n",world.room_count,faces,world.primary_count,world.child_count,world.allocated_bytes,world.peak_bytes,queries,hits,errors,hashes[0]);
        free(poison);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--world-sweep")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0},guard,sentinel;
        uint32_t i,j,k,q,pass,edge_hits=0,faces=0,queries=0,hits=0,errors=0,hashes[2]={2166136261u,2166136261u},geometry_bytes;void *poison=NULL;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        memset(&guard,0xa5,sizeof(guard));sentinel=guard;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes,&guard))return 6;
        rf_geometry_collision_world_close(&guard);
        for(i=0;i<world.room_count;i++) {
            const rf_collision_tree *tree=&world.rooms[i].tree;
            if(world.views[i].tree!=tree || memcmp(world.views[i].minimum,world.rooms[i].minimum,24))return 7;
            faces+=tree->face_count;
            for(j=0;j<tree->face_count;j++) {rf_geometry_face face;if(rf_geometry_get_face(&geometry,tree->source_indices[j],&face) || face.room!=i)return 8;}
        }
        if(faces!=geometry.faces)return 9;
        geometry_bytes=geometry.bytes;
        for(pass=0;pass<2;pass++) {
            for(i=0;i<world.room_count;i++)for(q=0;q<3;q++) {
                const rf_collision_tree *tree=&world.rooms[i].tree;const rf_collision_face *face;float start[3]={0},delta[3];
                struct {int32_t status;uint32_t matched;rf_geometry_world_sweep_hit hit;} out;const unsigned char *bytes=(const unsigned char*)&out;
                if(!tree->face_count)continue;face=tree->faces;
                for(j=0;j<face->count;j++)for(k=0;k<3;k++)start[k]+=face->vertices[j][k];
                for(k=0;k<3;k++) {
                    float anchor=q==0?start[k]/face->count:q==1?face->vertices[0][k]:(face->vertices[0][k]+face->vertices[1%face->count][k])*.5f;
                    start[k]=anchor+face->plane[k]+.0037f*(k+1);delta[k]=-2*face->plane[k]+.0013f*(k+1);
                }
                memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_world_sweep(&world,0x460,start,delta,.25f*(q+1),1,&out.hit,&out.matched);
                if(!pass) {queries++;errors+=out.status!=0;if(!out.status) {hits+=out.matched;if(out.matched)edge_hits+=out.hit.edge!=0;}}
                if(!out.status && out.matched) {
                    const rf_collision_tree *target;uint32_t found=0;
                    if(out.hit.face>=faces || out.hit.room>=world.room_count)return 10;
                    target=&world.rooms[out.hit.room].tree;
                    for(j=0;j<target->face_count;j++)if(target->source_indices[j]==out.hit.face)found=1;
                    if(!found)return 13;
                }
                for(j=0;j<sizeof(out);j++)hashes[pass]=(hashes[pass]^bytes[j])*16777619u;
            }
            if(!pass) {rf_geometry_close(&geometry);poison=malloc(geometry_bytes);if(!poison)return 11;memset(poison,0xdd,geometry_bytes);}
        }
        if(hashes[0]!=hashes[1])return 12;
        printf("%u %u %u %u %u %u %u %u %u %u %u\n",world.room_count,faces,world.primary_count,world.child_count,world.allocated_bytes,world.peak_bytes,queries,hits,errors,hashes[0],edge_hits);
        free(poison);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--transformed-rooms")) {
        struct {struct {float bounds[6],z;uint32_t skip,first,count;} rooms[4];uint32_t primary[2],children[4];float start[3],delta[3],limit;uint32_t flags;float radius,origin[3],matrix[3][3];} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_room_view rooms[4];rf_collision_tree trees[4];rf_collision_node nodes[4];rf_collision_face faces[4];float vertices[4][4][3];uint32_t stacks[4],i,j;
            struct {int32_t status;uint32_t matched;rf_collision_sweep_room_hit hit;} out;
            memset(trees,0,sizeof(trees));memset(faces,0,sizeof(faces));
            for(i=0;i<4;i++) {
                float z=in.rooms[i].z;
                memcpy(rooms[i].minimum,in.rooms[i].bounds,24);rooms[i].skip=in.rooms[i].skip;rooms[i].first_child=in.rooms[i].first;rooms[i].child_count=in.rooms[i].count;rooms[i].tree=trees+i;
                trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stacks+i;
                for(j=0;j<3;j++) {nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-2.0001f;nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:2.0001f;}
                nodes[i].first_face=0;nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_transformed_rooms(rooms,4,in.primary,2,in.children,4,in.flags,in.start,in.delta,in.origin,in.matrix,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sweep-rooms")) {
        struct {struct {float bounds[6],z;uint32_t skip,first,count;} rooms[4];uint32_t primary[2],children[4];float start[3],delta[3],limit;uint32_t flags;float radius;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_room_view rooms[4];rf_collision_tree trees[4];rf_collision_node nodes[4];rf_collision_face faces[4];float vertices[4][4][3];uint32_t stacks[4],i,j;
            struct {int32_t status;uint32_t matched;rf_collision_sweep_room_hit hit;} out;
            memset(trees,0,sizeof(trees));memset(faces,0,sizeof(faces));
            for(i=0;i<4;i++) {
                float z=in.rooms[i].z;
                memcpy(rooms[i].minimum,in.rooms[i].bounds,24);rooms[i].skip=in.rooms[i].skip;rooms[i].first_child=in.rooms[i].first;rooms[i].child_count=in.rooms[i].count;rooms[i].tree=trees+i;
                trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stacks+i;
                for(j=0;j<3;j++) {nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-2.0001f;nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:2.0001f;}
                nodes[i].first_face=0;nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_sweep_rooms(rooms,4,in.primary,2,in.children,4,in.flags,in.start,in.delta,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--room-query")) {
        struct {struct {float bounds[6],z;uint32_t skip,first,count;} rooms[4];uint32_t primary[2],children[4];float start[3],delta[3],limit;uint32_t flags;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_room_view rooms[4];rf_collision_tree trees[4];rf_collision_node nodes[4];rf_collision_face faces[4];float vertices[4][4][3];uint32_t stacks[4],i,j;
            struct {int32_t status;uint32_t matched;rf_collision_room_hit hit;} out;
            memset(trees,0,sizeof(trees));memset(faces,0,sizeof(faces));
            for(i=0;i<4;i++) {
                float z=in.rooms[i].z;
                memcpy(rooms[i].minimum,in.rooms[i].bounds,24);rooms[i].skip=in.rooms[i].skip;rooms[i].first_child=in.rooms[i].first;rooms[i].child_count=in.rooms[i].count;rooms[i].tree=trees+i;
                trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stacks+i;
                for(j=0;j<3;j++) {nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-2.0001f;nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:2.0001f;}
                nodes[i].first_face=0;nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_thin_rooms(rooms,4,in.primary,2,in.children,4,in.flags,in.start,in.delta,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && (!strcmp(argv[1],"--rooms") || !strcmp(argv[1],"--room-layout") || !strcmp(argv[1],"--room-layout-tight"))) {
        int tight=!strcmp(argv[1],"--room-layout-tight"),layout=strcmp(argv[1],"--rooms")!=0;
        rf_vpp archive;rf_level level;rf_geometry geometry;uint32_t room,total=0,nodes=0,peak=0,queries=0,hits=0;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        for(room=0;room<geometry.rooms;room++) {
            rf_geometry_collision_room owned={0},guard,sentinel;uint32_t i,j,k;
            if(tight)memset(geometry.data+geometry.room_offsets[room]+4,0,24);
            if(rf_geometry_collision_room_open(&geometry,room,8u*1024u*1024u,&owned))return 4;
            memset(&guard,0xa5,sizeof(guard));sentinel=guard;
            if(rf_geometry_collision_room_open(&geometry,room,owned.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            if(rf_geometry_collision_room_open(&geometry,room,owned.peak_bytes,&guard))return 6;
            rf_geometry_collision_room_close(&guard);
            total+=owned.tree.face_count;nodes+=owned.tree.node_count;if(owned.peak_bytes>peak)peak=owned.peak_bytes;
            if(layout) {
                if(fwrite(geometry.data+geometry.room_offsets[room]+4,24,1,stdout)!=1 || fwrite(owned.minimum,24,1,stdout)!=1 || fwrite(&owned.tree.face_count,4,1,stdout)!=1)return 14;
                for(i=0;i<owned.tree.face_count;i++)if(fwrite(owned.tree.source_indices+i,4,1,stdout)!=1 || fwrite(owned.tree.faces[i].minimum,24,1,stdout)!=1)return 14;
                rf_geometry_collision_room_close(&owned);continue;
            }
            for(i=0;i<owned.tree.face_count;i++) {
                rf_geometry_face original;rf_collision_face *face=owned.tree.faces+i;
                float start[3]={0},delta[3],limit=1;uint32_t matched,brute=0;
                rf_collision_tree_hit result;rf_collision_ray_hit hit;
                if(rf_geometry_get_face(&geometry,owned.tree.source_indices[i],&original) || original.room!=room || original.corners!=face->count)return 7;
                for(j=0;j<i;j++)if(owned.tree.source_indices[j]==owned.tree.source_indices[i])return 8;
                for(j=0;j<face->count;j++) {
                    rf_geometry_corner corner;float position[3];
                    if(rf_geometry_get_corner(&geometry,owned.tree.source_indices[i],j,&corner) || rf_geometry_vertex(&geometry,corner.vertex,position) || memcmp(position,face->vertices[j],12))return 9;
                    for(k=0;k<3;k++)start[k]+=position[k];
                }
                /* Skew synthetic rays to avoid coplanar parallel rays against
                 * adjacent faces (the primitive intentionally preserves NaN). */
                for(j=0;j<3;j++) {start[j]=start[j]/face->count+face->plane[j]+0.0037f*(j+1);delta[j]=-2*face->plane[j]+0.0013f*(j+1);}
                /* Nearest mode: equal-depth identities can differ with traversal order. */
                for(j=0;j<owned.tree.face_count;j++) {
                    rf_collision_face candidate=owned.tree.faces[j];uint32_t accepted;
                    candidate.filter.query_flags=0x460;
                    {int status=rf_collision_thin_face(&candidate,start,delta,limit,&hit,&accepted);if(status) {fprintf(stderr,"room %u ray %u candidate %u status %d\n",room,i,j,status);return 10;}}
                    if(accepted) {brute=1;limit=hit.fraction;}
                }
                if(rf_collision_thin_tree(owned.tree.nodes,owned.tree.node_count,owned.tree.faces,owned.tree.face_count,0x460,start,delta,1,owned.tree.stack,owned.tree.node_capacity,&result,&matched))return 11;
                if(matched!=brute || (matched && result.hit.fraction!=limit))return 12;
                queries++;hits+=matched;
            }
            rf_geometry_collision_room_close(&owned);
        }
        if(total!=geometry.faces)return 13;
        if(!layout)printf("%u %u %u %u %u %u\n",geometry.rooms,total,nodes,peak,queries,hits);
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--level") || !strcmp(argv[1],"--level-initial"))) {
        int initial=!strcmp(argv[1],"--level-initial");
        rf_vpp archive;rf_level level;rf_geometry geometry;float scratch[256][3];
        rf_collision_face_filter filter={0x461,0,0,0,0,0};uint32_t index,j,k;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        if(rf_level_open(&level,&archive,argv[3])) {rf_vpp_close(&archive);return 3;}
        if(rf_geometry_open(&geometry,&level,8u*1024u*1024u)) {rf_vpp_close(&archive);return 3;}
        for(index=0;index<geometry.faces;index++) {
            rf_collision_face face,guard,sentinel;float start[3]={0},delta[3];
            struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
            if(initial && rf_geometry_initial_collision_filter(&geometry,index,0x461,&filter))return 6;
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,256,&face))return 4;
            memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,face.count-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            for(j=0;j<3;j++) {
                for(k=0;k<face.count;k++)start[j]+=scratch[k][j];
                start[j]=start[j]/face.count+face.plane[j];delta[j]=-2*face.plane[j];
            }
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_face(&face,start,delta,1,&out.hit,&out.matched);
            if(initial && fwrite(&filter,sizeof(filter),1,stdout)!=1)return 2;
            if(fwrite(&index,4,1,stdout)!=1 || fwrite(&face.count,4,1,stdout)!=1 || fwrite(face.plane,4,4,stdout)!=4 ||
               fwrite(face.minimum,4,3,stdout)!=3 || fwrite(face.maximum,4,3,stdout)!=3 || fwrite(scratch,12,face.count,stdout)!=face.count ||
               fwrite(start,4,3,stdout)!=3 || fwrite(delta,4,3,stdout)!=3 || fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--build")) {
        struct {float faces[16][6];uint32_t count,budget;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[16];rf_collision_tree tree={0};uint32_t i;int32_t status;
            memset(faces,0,sizeof(faces));for(i=0;i<16;i++)memcpy(faces[i].minimum,in.faces[i],24);
            status=in.count>16?RF_RANGE:rf_collision_tree_open(faces,in.count,in.budget,&tree);
            if(status) {
                rf_collision_tree guard,sentinel;
                memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
                if(in.count<=16 && (rf_collision_tree_open(faces,in.count,in.budget,&guard)!=status || memcmp(&guard,&sentinel,sizeof(guard))))return 7;
            }
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&tree.node_count,4,1,stdout)!=1 || fwrite(&tree.peak_bytes,4,1,stdout)!=1)return 2;
            if(!status) {
                if(fwrite(tree.nodes,sizeof(*tree.nodes),tree.node_count,stdout)!=tree.node_count || fwrite(tree.source_indices,4,in.count,stdout)!=in.count)return 2;
            }
            rf_collision_tree_close(&tree);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--partition")) {
        struct {float bounds[6],faces[8][6];} in;
        struct {int32_t status;uint32_t axis,counts[3];uint8_t labels[8];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_node node;rf_collision_face faces[8];unsigned i;
            memcpy(node.minimum,in.bounds,24);
            for(i=0;i<8;i++)memcpy(faces[i].minimum,in.faces[i],24);
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_partition(&node,faces,8,out.labels,&out.axis,out.counts);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sweep-tree")) {
        struct {rf_collision_node nodes[3];float z[3],start[3],delta[3],limit;uint32_t flags;float normal_delta[3],radius;} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_tree_hit result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[3];float vertices[3][4][3];uint32_t stack[3],i,j;
            for(i=0;i<3;i++) {
                static const float xy[4][2]={{-2,-2},{2,-2},{2,2},{-2,2}};
                memset(faces+i,0,sizeof(faces[i]));faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];
                faces[i].minimum[0]=faces[i].minimum[1]=-2.0001f;faces[i].maximum[0]=faces[i].maximum[1]=2.0001f;
                faces[i].minimum[2]=in.z[i]-.0001f;faces[i].maximum[2]=in.z[i]+.0001f;
                for(j=0;j<4;j++) {vertices[i][j][0]=xy[j][0];vertices[i][j][1]=xy[j][1];vertices[i][j][2]=in.z[i];}
                faces[i].vertices=vertices[i];faces[i].count=4;
            }
            memset(&out.result,0xa5,sizeof(out.result));out.matched=0xa5a5a5a5;
            out.status=rf_collision_sweep_tree(in.nodes,3,faces,3,in.flags,in.start,in.delta,in.normal_delta,in.radius,in.limit,stack,3,&out.result,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--tree")) {
        struct {rf_collision_node nodes[3];float z[3],start[3],delta[3],limit;uint32_t flags;} in;
        struct {int32_t status;uint32_t matched;rf_collision_tree_hit result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[3];float vertices[3][4][3];uint32_t stack[3],i,j;
            for(i=0;i<3;i++) {
                static const float xy[4][2]={{-2,-2},{2,-2},{2,2},{-2,2}};
                memset(faces+i,0,sizeof(faces[i]));faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];
                faces[i].minimum[0]=faces[i].minimum[1]=-2.0001f;faces[i].maximum[0]=faces[i].maximum[1]=2.0001f;
                faces[i].minimum[2]=in.z[i]-.0001f;faces[i].maximum[2]=in.z[i]+.0001f;
                for(j=0;j<4;j++) {vertices[i][j][0]=xy[j][0];vertices[i][j][1]=xy[j][1];vertices[i][j][2]=in.z[i];}
                faces[i].vertices=vertices[i];faces[i].count=4;
            }
            memset(&out.result,0xa5,sizeof(out.result));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_tree(in.nodes,3,faces,3,in.flags,in.start,in.delta,in.limit,stack,3,&out.result,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sweep")) {
        struct {float plane[4],lo[3],hi[3],vertices[8][3],start[3],delta[3],limit;rf_collision_face_filter filter;uint32_t count;float normal_delta[3],radius;} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face face;
            memcpy(face.plane,in.plane,16);memcpy(face.minimum,in.lo,12);memcpy(face.maximum,in.hi,12);
            face.vertices=in.vertices;face.count=in.count;face.filter=in.filter;
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=in.count>8?RF_RANGE:rf_collision_sweep_face(&face,in.start,in.delta,in.normal_delta,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--thin")) {
        struct {float plane[4],lo[3],hi[3],vertices[8][3],start[3],delta[3],limit;rf_collision_face_filter filter;uint32_t count;} in;
        struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face face;
            memcpy(face.plane,in.plane,16);memcpy(face.minimum,in.lo,12);memcpy(face.maximum,in.hi,12);
            face.vertices=in.vertices;face.count=in.count;face.filter=in.filter;
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=in.count>8?RF_RANGE:rf_collision_thin_face(&face,in.start,in.delta,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--filter")) {
        rf_collision_face_filter in;struct {int32_t status;uint32_t accepted;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.accepted=0xa5a5a5a5;out.status=rf_collision_face_accept(&in,&out.accepted);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--polygon")) {
        struct {float normal[3],point[3],vertices[16][3];uint32_t count;} in;
        struct {int32_t status;uint32_t inside;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.inside=0xa5a5a5a5;
            out.status=in.count>16?RF_RANGE:rf_collision_polygon_contains(in.normal,in.point,in.vertices,in.count,&out.inside);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--plane")) {
        struct {float start[3],delta[3],plane[4],fraction;} in;
        struct {int32_t status;uint32_t hit;float fraction;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.hit=0xa5a5a5a5;out.fraction=in.fraction;
            out.status=rf_collision_segment_plane(in.start,in.delta,in.plane,&out.fraction,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    while(fread(&input,sizeof(input),1,stdin)==1) {
        unsigned i;output.hit=0xa5a5a5a5;for(i=0;i<3;i++)output.point[i]=input.point[i];
        output.status=rf_collision_segment_box(input.lo,input.hi,input.start,input.end,output.point,&output.hit);
        if(fwrite(&output,sizeof(output),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}
