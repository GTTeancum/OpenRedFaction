/* Offline replay of settled DEV checkpoint admissions, including rejected cuts. */
#include "../src/core/geomod.c"
#include "rf/geometry.h"
#define SNAPSHOT_VERTICES 8192
#define SNAPSHOT_FACES 2048
#include "../tests/geomod_lineage_coverage.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"probe line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geomod_template shape;
    rf_geomod_vertex vertices[24];rf_geomod_face faces[6];rf_collision_face_filter filters[6],generated={0};
    rf_geomod_terrain *terrain=NULL;rf_geomod_mesh_view source;rf_random_state random={1};
    unsigned char *data;FILE *file;long length;uint32_t i,j,n,offset,cursor=316,first_bad=0;char path[1024];
    CHECK(argc==4);file=fopen(argv[3],"rb");CHECK(file && !fseek(file,0,SEEK_END));length=ftell(file);
    CHECK(length>=288 && length<=262144 && !fseek(file,0,SEEK_SET));data=malloc(length);CHECK(data && fread(data,1,length,file)==(size_t)length);fclose(file);
    CHECK(!memcmp(data,"RFDS",4) && geomod_u32(data+4)==1 && geomod_u32(data+8)==(uint32_t)length);
    n=geomod_u32(data+240);offset=288+geomod_u32(data+252);CHECK(n<=128 && offset<=(uint32_t)length && n*48<=(uint32_t)length-offset);
    snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
    CHECK(!rf_level_open(&level,&archive,"glass_house.rfl") && !rf_geometry_open(&geometry,&level,8*1024*1024));
    for(i=0;i<6;i++) {
        rf_geometry_face f;CHECK(!rf_geometry_get_face(&geometry,i,&f) && f.corners==4 && !f.room);
        faces[i]=(rf_geomod_face){i*4,4,f.texture,i};CHECK(!rf_geometry_initial_collision_filter(&geometry,i,0,filters+i));
        for(j=0;j<4;j++){rf_geometry_corner c;CHECK(!rf_geometry_get_corner(&geometry,i,j,&c));CHECK(!rf_geometry_vertex(&geometry,c.vertex,vertices[i*4+j].position));memcpy(vertices[i*4+j].uv,c.uv,8);}
    }
    source=(rf_geomod_mesh_view){vertices,faces,24,6,0};generated.face_flags=256;
    CHECK(!rf_geomod_template_load(argv[2],&shape));
    CHECK(!rf_geomod_terrain_open(&source,filters,&generated,1,8192,2048,2097152,&terrain));
    CHECK(!rf_geomod_terrain_set_mapping(terrain,geomod_u32(data+208),geomod_u32(data+212)));
    for(i=0;i<n;i++) {
        float center[3],basis[9],scale;rf_geomod_terrain_view view;int status;
        const unsigned char *record=data+offset+i*48;
        for(j=0;j<3;j++)center[j]=geomod_float(record+j*4);
        for(j=0;j<6;j++)CHECK(geomod_float(record+12+j*4)==0); /* Unconstrained fixture only. */
        scale=geomod_float(record+36);CHECK(!rf_geomod_random_basis(&random,basis));
        status=rf_geomod_terrain_cut_template_scale(terrain,&shape,center,basis,scale,77);
        CHECK(!rf_geomod_terrain_get(terrain,&view));
        if(!status) {
            const rf_geomod_mesh_view *cut=terrain->cuts+terrain->count-1;uint32_t bytes=cut->vertex_count*20;
            CHECK(cursor+24<=offset && geomod_u32(data+cursor+4)==cut->vertex_count && geomod_u32(data+cursor+8)==cut->face_count);
            CHECK(cursor+24+bytes+cut->face_count*16<=offset);
            CHECK(!memcmp(data+cursor+12,terrain->kernels[terrain->count-1],12));
            CHECK(!memcmp(data+cursor+24,cut->vertices,bytes));
            cursor+=24+bytes+cut->face_count*16;
        }

        if(!status && getenv("RF_GEOMOD_PROBE_MESH_PREFIX")) {
            char name[2048];uint32_t counts[2]={view.mesh.vertex_count,view.mesh.face_count};FILE *out;
            CHECK(snprintf(name,sizeof(name),"%s-%02u.mesh",getenv("RF_GEOMOD_PROBE_MESH_PREFIX"),i+1)>0);
            out=fopen(name,"wb");CHECK(out);
            CHECK(fwrite("RGM1",1,4,out)==4 && fwrite(counts,4,2,out)==2);
            CHECK(fwrite(view.mesh.vertices,sizeof(*view.mesh.vertices),counts[0],out)==counts[0]);
            CHECK(fwrite(view.mesh.faces,sizeof(*view.mesh.faces),counts[1],out)==counts[1]);CHECK(!fclose(out));
        }
        printf("ADMISSION %u status%d cuts%u vertices%u faces%u peak%u\n",i+1,status,view.cuts,view.mesh.vertex_count,view.mesh.face_count,view.peak_bytes);
        if(!status && getenv("RF_GEOMOD_PROBE_CLOSURE")) {
            int closed;report_closure=1;closed=lineage_closed(&view.mesh);
            printf("PREFIX_CLOSURE admission%u cuts%u closed%d\n",i+1,view.cuts,closed);
            if(!closed && !first_bad)first_bad=i+1;
        }
        if(status && terrain->count<RF_GEOMOD_CUT_LIMIT && terrain->cuts[terrain->count].face_count) {
            int stage=terrain_prepare_chronological_mesh(terrain,terrain->count+1);
            printf("REJECT_STAGE admission%u chronological%d\n",i+1,stage);rf_geomod_storage_abort(terrain->mesh);
        }
    }
    CHECK(cursor==offset && terrain->count==geomod_u32(data+300) && random.value==geomod_u32(data+244));
    puts("MATCH all committed cutter positions/UVs/kernels and final admission RNG; rejected statuses remain diagnostic findings");
    rf_geomod_terrain_close(&terrain);rf_geometry_close(&geometry);rf_vpp_close(&archive);free(data);
    if(first_bad)fprintf(stderr,"FIRST_NONCLOSED_ADMISSION %u\n",first_bad);return first_bad?2:0;
}
