/* Internal two-cut chronological solid prototype; no public/live-path claim. */
/* Retain independent legacy query comparisons after live activation. */
#define RF_GEOMOD_LEGACY_REPLAY_TEST 1
#include "../src/core/geomod.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geomod_lineage_coverage.inc"
#include "geomod_lineage_collision.inc"

static void box(const float lo[3],const float hi[3],int inward,
    rf_geomod_vertex *v,rf_geomod_face *f)
{
    static const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
    unsigned axis,side,j;
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
        unsigned face=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        f[face]=(rf_geomod_face){face*4,4,5,face};
        for(j=0;j<4;j++) {
            unsigned k=side?j:3-j;if(inward)k=3-k;
            memset(v+face*4+j,0,sizeof(*v));
            v[face*4+j].position[axis]=side?hi[axis]:lo[axis];
            v[face*4+j].position[a]=u[k]>0?hi[a]:lo[a];
            v[face*4+j].position[b]=w[k]>0?hi[b]:lo[b];
        }
    }
}
static int target(const rf_geomod_mesh_view *m,const rf_geomod_face *f)
{
    unsigned i;if(f->material!=47 || f->source_face!=UINT32_MAX)return 0;
    for(i=0;i<f->count;i++) {
        const float *p=m->vertices[f->first+i].position;
        if(fabs((double)p[0]+p[1]-10.5)>1e-5)return 0;
    }
    return 1;
}
static int run(unsigned cavity,float roof,float split,unsigned *comparisons,unsigned *changed)
{
    rf_geomod_vertex sourcev[24],quad[24],cutv[36],saved[256];
    rf_geomod_face sourcef[6],qf[6],cutf[12];
    rf_collision_face_filter filters[6]={{0}},generated={0};
    rf_geomod_mesh_view source={sourcev,sourcef,24,6,1},cut={cutv,cutf,36,12,1};
    rf_geomod_terrain *t=NULL;rf_geomod_terrain_view view;
    const float lo[3]={-4,6,-4},local_lo[3]={-1,-1,-1},local_hi[3]={1,0,1};
    float hi[3]={4,14,4},kernel[3]={.25f,9.75f,0};
    float center[3]={(split-2)*.5f,10,0},extent[3]={(split+2)*.5f,4,2};
    unsigned f,j,k,n=0;int status,baseline_closed;const char *stage="open";
    if(cavity)hi[2]=roof;
    box(lo,hi,(int)cavity,sourcev,sourcef);box(local_lo,local_hi,0,quad,qf);
    /* Positive-determinant shear. The outward +v face becomes x+y=10.5;
     * its exposed inward normal has an exact X/Y tie before the next cut. */
    for(f=0;f<6;f++)for(j=0;j<2;j++) {
        static const unsigned corners[2][3]={{0,1,2},{0,2,3}};
        unsigned id=f*2+j;cutf[id]=(rf_geomod_face){id*3,3,47,UINT32_MAX};
        for(k=0;k<3;k++) {
            rf_geomod_vertex a=quad[f*4+corners[j][k]];
            cutv[id*3+k]=a;cutv[id*3+k].position[0]=a.position[0]+.25f;
            cutv[id*3+k].position[1]=10.25f-a.position[0]+a.position[1];
        }
    }
    status=rf_geomod_terrain_open(&source,filters,&generated,cavity,4096,800,1048576,&t);
    if(status)goto done;
    status=rf_geomod_terrain_set_mapping(t,256,256);if(status)goto done;
    stage="first-star";status=rf_geomod_terrain_cut_star(t,&cut,kernel);if(status)goto done;
    status=rf_geomod_terrain_get(t,&view);if(status)goto done;
    for(f=0;f<view.mesh.face_count;f++)if(target(&view.mesh,view.mesh.faces+f)) {
        const rf_geomod_face *face=view.mesh.faces+f;
        if(face->count>256-n){status=-2;goto done;}
        memcpy(saved+n,view.mesh.vertices+face->first,face->count*sizeof(*saved));n+=face->count;
    }
    if(!n){stage="no-first-target";status=-2;goto done;}
    stage="second-box";status=rf_geomod_terrain_cut_box(t,center,extent,48);if(status)goto done;
    status=rf_geomod_terrain_get(t,&view);if(status)goto done;
    baseline_closed=lineage_closed(&view.mesh);printf("SOLID_FULL_REBUILD_COVERAGE %d\n",baseline_closed);
    /* Standalone chronological reconstruction from immutable source and the
     * actual two-cut history. Live terrain remains unchanged. */
    {
        rf_geomod_terrain *replay=calloc(1,sizeof(*replay));
        geomod_face_lineage *tags=calloc(1,sizeof(*tags));unsigned prefix;
        geomod_step_support *provenance=calloc(1,sizeof(*provenance));
        rf_geomod_mesh_view reconstructed;
        if(!replay || !tags || !provenance){free(replay);free(tags);free(provenance);status=RF_IO;goto done;}
        replay->mapping_width=replay->mapping_height=256;
        status=rf_geomod_storage_open(&source,4096,800,1048576,&replay->mesh);
        memcpy(replay->work.cut_planes,t->work.cut_planes,sizeof(t->work.cut_planes));
        memcpy(replay->work.star_planes,t->work.star_planes,sizeof(t->work.star_planes));
        memcpy(replay->work.star_count,t->work.star_count,sizeof(t->work.star_count));
        for(prefix=1;!status && prefix<=2;prefix++) {
            status=prepare_chronological_step(replay->mesh,t->cuts,prefix,&replay->work,tags,provenance,cavity);
            if(!status)status=terrain_map_pending_lineage(replay,tags);
            if(!status)status=rf_geomod_storage_commit(replay->mesh);
        }
        if(!status)status=rf_geomod_storage_view(replay->mesh,&reconstructed);
        if(!status){int closed=lineage_closed(&reconstructed);printf("SOLID_CHRONOLOGICAL_COVERAGE %d\n",closed);if(!closed)status=RF_FORMAT;}
        if(!status)status=lineage_collision_equal(&view.mesh,&reconstructed);
        if(!status) {
            rf_geomod_mesh_view pending,unchanged;uint32_t budget=t->budget;
            t->budget=t->base_bytes;
            if(terrain_prepare_chronological_mesh(t,2)!=RF_RANGE)status=RF_FORMAT;
            t->budget=budget;
            if(!status)status=rf_geomod_storage_view(t->mesh,&unchanged);
            if(!status && (unchanged.vertices!=view.mesh.vertices || unchanged.generation!=view.mesh.generation || t->mesh->editing))status=RF_FORMAT;
            if(!status)status=terrain_prepare_chronological_mesh(t,2);
            if(!status)status=rf_geomod_storage_pending(t->mesh,&pending);
            if(!status && (pending.vertex_count!=reconstructed.vertex_count || pending.face_count!=reconstructed.face_count ||
                memcmp(pending.vertices,reconstructed.vertices,pending.vertex_count*sizeof(*pending.vertices)) ||
                memcmp(pending.faces,reconstructed.faces,pending.face_count*sizeof(*pending.faces))))status=RF_FORMAT;
            rf_geomod_storage_abort(t->mesh);
        }

        if(!status) {
            /* Keep result alive for existing exact-corner comparisons below. */
            rf_geomod_storage_close(&t->mesh);t->mesh=replay->mesh;replay->mesh=NULL;
            view.mesh=reconstructed;
        }
        rf_geomod_storage_close(&replay->mesh);free(replay);free(tags);free(provenance);
        if(status)goto done;
    }

    for(f=0;f<view.mesh.face_count;f++)if(target(&view.mesh,view.mesh.faces+f)) {
        const rf_geomod_face *face=view.mesh.faces+f;
        for(j=0;j<face->count;j++) {
            const rf_geomod_vertex *v=view.mesh.vertices+face->first+j;
            for(k=0;k<n;k++)if(!memcmp(v->position,saved[k].position,12)) {
                ++*comparisons;
                if(memcmp(v->uv,saved[k].uv,8)) {
                    ++*changed;
                    printf("PUBLIC_UV_CHANGED cavity=%u roof=%.9g split=%.9g xyz=%.9g,%.9g,%.9g old=%.9g,%.9g new=%.9g,%.9g cuts=%u faces=%u\n",
                        cavity,roof,split,v->position[0],v->position[1],v->position[2],
                        saved[k].uv[0],saved[k].uv[1],v->uv[0],v->uv[1],view.cuts,view.mesh.face_count);
                }
                break;
            }
        }
    }
done:
    printf("PUBLIC_UV_CASE cavity=%u roof=%.9g split=%.9g status=%d stage=%s comparisons=%u changed=%u\n",
        cavity,roof,split,status,stage,*comparisons,*changed);
    rf_geomod_terrain_close(&t);return status;
}
int main(void)
{
    static const float splits[]={0,.1f,.2f,.3f},roofs[]={-.75f,0,.25f};
    unsigned c,r,x,total=0,changed=0,accepted=0,rejected=0;
    for(c=0;c<2;c++)for(r=0;r<(c?3u:1u);r++)for(x=0;x<4;x++) {
        unsigned compared=0,delta=0;int status=run(c,roofs[r],splits[x],&compared,&delta);
        if(status)++rejected;else ++accepted;
        total+=compared;changed+=delta;
    }
    printf("PUBLIC_UV_SUMMARY accepted=%u rejected=%u comparisons=%u changed=%u\n",accepted,rejected,total,changed);
    if(changed)return 1;
    if(accepted!=16 || rejected || total<8)return 2;
    puts("PASS solid/cavity UV/closed-coverage/collision comparison; live integration pending");return 0;
}
