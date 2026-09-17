#include "rf/geomod_piece_bank.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
struct rf_geomod_piece_bank {
    rf_geomod_owned_piece *pieces;rf_geomod_vertex *vertices;rf_geomod_face *faces;
    uint32_t *old_faces;rf_collision_face_filter *filters;
    rf_collision_face *collision;float (*positions)[3];
    uint32_t vc,fc,pc,nv,nf,np,bytes;
};
int rf_geomod_piece_bank_open(uint32_t vc,uint32_t fc,uint32_t pc,uint32_t budget,rf_geomod_piece_bank **out)
{
    uint64_t bytes;rf_geomod_piece_bank *bank;unsigned char *p;
    if(!out || *out || !vc || !fc || !pc)return RF_RANGE;
    bytes=sizeof(*bank)+(uint64_t)pc*sizeof(rf_geomod_owned_piece)+(uint64_t)vc*(sizeof(rf_geomod_vertex)+sizeof(*bank->positions))+
        (uint64_t)fc*(sizeof(rf_geomod_face)+sizeof(uint32_t)+sizeof(rf_collision_face_filter)+sizeof(rf_collision_face));
    if(bytes>budget || bytes>UINT32_MAX)return RF_RANGE;
    bank=calloc(1,(size_t)bytes);if(!bank)return RF_IO;
    p=(unsigned char *)(bank+1);bank->pieces=(rf_geomod_owned_piece *)p;p+=pc*sizeof(*bank->pieces);
    /* Pointer-bearing collision records precede float/word arrays so native
     * alignment is retained on both32-bit Xbox and64-bit PC. */
    bank->collision=(rf_collision_face *)p;p+=fc*sizeof(*bank->collision);
    bank->positions=(float (*)[3])p;p+=vc*sizeof(*bank->positions);
    bank->vertices=(rf_geomod_vertex *)p;p+=vc*sizeof(*bank->vertices);
    bank->faces=(rf_geomod_face *)p;p+=fc*sizeof(*bank->faces);
    bank->old_faces=(uint32_t *)p;p+=fc*sizeof(*bank->old_faces);
    bank->filters=(rf_collision_face_filter *)p;
    bank->vc=vc;bank->fc=fc;bank->pc=pc;bank->bytes=(uint32_t)bytes;*out=bank;return RF_OK;
}
void rf_geomod_piece_bank_close(rf_geomod_piece_bank **bank)
{if(bank){free(*bank);*bank=NULL;}}
uint32_t rf_geomod_piece_bank_count(const rf_geomod_piece_bank *bank)
{return bank?bank->np:0;}
uint32_t rf_geomod_piece_bank_bytes(const rf_geomod_piece_bank *bank)
{return bank?bank->bytes:0;}
int rf_geomod_piece_bank_get(const rf_geomod_piece_bank *bank,uint32_t index,rf_geomod_owned_piece *piece)
{
    if(!bank || !piece || index>=bank->np)return RF_RANGE;
    *piece=bank->pieces[index];return RF_OK;
}
/* A float center translation can bend an almost-collinear polygon past the
 * convex-face gate. Preserve its exact boundary by partitioning only rejected
 * polygons into center-fan triangles; never weld or move the boundary corners. */
static int rebind_translated_piece(rf_geomod_piece_bank *bank,rf_geomod_owned_piece *piece)
{
    uint8_t split[RF_GEOMOD_WORK_FACES]={0};uint32_t i,j,k,nv=0,nf=0,total_faces;int status;
    rf_geomod_vertex *vertices=bank->vertices+bank->nv;rf_geomod_face *faces=bank->faces+bank->nf;
    rf_collision_face_filter *filters=bank->filters+bank->nf;uint32_t *old=bank->old_faces+bank->nf;
    if(piece->mesh.face_count>RF_GEOMOD_WORK_FACES)return RF_RANGE;
    for(i=0;i<piece->mesh.face_count;i++) {
        rf_geomod_face f=faces[i];rf_collision_face bound;rf_geomod_mesh_view one={vertices+f.first,&f,f.count,1,0};f.first=0;
        status=rf_geomod_collision_faces(&one,filters+i,bank->positions+bank->nv,f.count,&bound,1);
        if(status && (status!=RF_FORMAT || f.count<4))return status;
        split[i]=status!=0;nv+=split[i]?3*f.count:f.count;nf+=split[i]?f.count:1;
    }
    if(nv>bank->vc-bank->nv || nf>bank->fc-bank->nf)return RF_RANGE;
    piece->mesh.vertex_count=nv;total_faces=nf;
    for(i=piece->mesh.face_count;i-->0;) {
        rf_geomod_face f=faces[i];rf_collision_face_filter filter=filters[i];uint32_t old_face=old[i];
        if(split[i]) {
            rf_geomod_vertex original[64],center={0};double sum[5]={0};
            memcpy(original,vertices+f.first,f.count*sizeof(*original));
            for(j=0;j<f.count;j++){for(k=0;k<3;k++)sum[k]+=original[j].position[k];for(k=0;k<2;k++)sum[k+3]+=original[j].uv[k];}
            for(k=0;k<3;k++)center.position[k]=(float)(sum[k]/f.count);for(k=0;k<2;k++)center.uv[k]=(float)(sum[k+3]/f.count);
            nv-=3*f.count;nf-=f.count;
            for(j=0;j<f.count;j++) {
                uint32_t at=nv+3*j;vertices[at]=center;vertices[at+1]=original[j];vertices[at+2]=original[(j+1)%f.count];
                faces[nf+j]=(rf_geomod_face){at,3,f.material,f.source_face};filters[nf+j]=filter;old[nf+j]=old_face;
            }
        } else {
            nv-=f.count;nf--;memmove(vertices+nv,vertices+f.first,f.count*sizeof(*vertices));f.first=nv;faces[nf]=f;filters[nf]=filter;old[nf]=old_face;
        }
    }
    piece->mesh.face_count=total_faces;
    return rf_geomod_collision_faces(&piece->mesh,filters,bank->positions+bank->nv,piece->mesh.vertex_count,
        bank->collision+bank->nf,piece->mesh.face_count);
}
static int append_piece(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id,const float *density)
{
    rf_geomod_owned_piece piece;uint32_t i,j,packed=0;int status;
    if(!bank || !mesh || !mesh->vertices || !mesh->faces || !old_faces || !filters || !mesh->vertex_count || !mesh->face_count)return RF_RANGE;
    if(bank->np==bank->pc || mesh->vertex_count>bank->vc-bank->nv || mesh->face_count>bank->fc-bank->nf)return RF_RANGE;
    for(i=0;i<bank->np;i++)if(bank->pieces[i].id==id)return RF_FORMAT;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *face=mesh->faces+i;
        if(face->first!=packed || face->count<3 || face->count>mesh->vertex_count-packed || old_faces[i]>=source_count)return RF_FORMAT;
        packed+=face->count;
    }
    if(packed!=mesh->vertex_count)return RF_FORMAT;
    for(i=0;i<mesh->vertex_count;i++)for(j=0;j<2;j++)if(!isfinite(mesh->vertices[i].uv[j]))return RF_FORMAT;
    memset(&piece,0,sizeof(piece));
    status=rf_geomod_mesh_recenter(mesh->vertices,mesh->vertex_count,bank->vertices+bank->nv,&piece.placement);if(status)return status;
    memcpy(bank->faces+bank->nf,mesh->faces,mesh->face_count*sizeof(*mesh->faces));
    memcpy(bank->old_faces+bank->nf,old_faces,mesh->face_count*sizeof(*old_faces));
    for(i=0;i<mesh->face_count;i++)bank->filters[bank->nf+i]=filters[old_faces[i]];
    piece.mesh=(rf_geomod_mesh_view){bank->vertices+bank->nv,bank->faces+bank->nf,mesh->vertex_count,mesh->face_count,mesh->generation};
    piece.old_faces=bank->old_faces+bank->nf;piece.filters=bank->filters+bank->nf;piece.id=id;
    status=rf_geomod_collision_faces(&piece.mesh,piece.filters,bank->positions+bank->nv,mesh->vertex_count,
        bank->collision+bank->nf,mesh->face_count);if(status)return status;
    piece.collision=bank->collision+bank->nf;
    piece.birth_radius=piece.placement.radius;
    if(density) {
        status=rf_physics_solid_mass_prepare(piece.collision,mesh->face_count,piece.placement.minimum,
            piece.placement.maximum,*density,&piece.mass);if(status)return status;
        for(i=0;i<mesh->vertex_count;i++)for(j=0;j<3;j++)
            bank->vertices[bank->nv+i].position[j]=(float)((double)bank->vertices[bank->nv+i].position[j]-piece.mass.center[j]);
        for(j=0;j<3;j++) {
            piece.placement.origin[j]=(float)((double)piece.placement.origin[j]+piece.mass.center[j]);
            piece.placement.minimum[j]=(float)((double)piece.placement.minimum[j]-piece.mass.center[j]);
            piece.placement.maximum[j]=(float)((double)piece.placement.maximum[j]-piece.mass.center[j]);
            if(!isfinite(piece.placement.origin[j]))return RF_RANGE;
        }
        piece.placement.radius=(float)((double)piece.placement.radius+sqrt(
            ((double)piece.mass.center[0]*piece.mass.center[0]+(double)piece.mass.center[1]*piece.mass.center[1])+
            (double)piece.mass.center[2]*piece.mass.center[2]));
        if(!isfinite(piece.placement.radius))return RF_RANGE;
        status=rf_geomod_collision_faces(&piece.mesh,piece.filters,bank->positions+bank->nv,mesh->vertex_count,
            bank->collision+bank->nf,mesh->face_count);
        if(status==RF_FORMAT)status=rebind_translated_piece(bank,&piece);
        if(status)return status;
        piece.mass_ready=1;
    }
    bank->pieces[bank->np++]=piece;bank->nv+=piece.mesh.vertex_count;bank->nf+=piece.mesh.face_count;
    return RF_OK;
}

int rf_geomod_piece_bank_append(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id)
{return append_piece(bank,mesh,old_faces,filters,source_count,id,NULL);}
int rf_geomod_piece_bank_append_physical(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id,float density)
{return append_piece(bank,mesh,old_faces,filters,source_count,id,&density);}
int rf_geomod_piece_body_open(const rf_geomod_owned_piece *piece,float elasticity,float friction,
    uint32_t budget,rf_physics_body *body)
{
    rf_physics_body_parameters p={0};rf_physics_sphere spheres[64];uint32_t count;float radius;int status;
    if(!piece || !piece->mass_ready || !body)return RF_RANGE;
    status=rf_physics_grid_spheres(piece->mass.cells,piece->mass.spacing,piece->mass.origin,spheres,64,&count,&radius);
    if(status)return status;
    p.coefficients[0]=elasticity;p.coefficients[1]=(float)((double)piece->birth_radius*(double).2f);p.coefficients[2]=friction;
    p.mass=piece->mass.mass;p.flags=0x8000003f;
    memcpy(p.local_tensor,piece->mass.inverse_tensor,36);memcpy(p.position,piece->placement.origin,12);
    p.orientation[0]=p.orientation[4]=p.orientation[8]=1;
    return rf_physics_body_open(&p,spheres,count,budget,body);
}

struct rf_geomod_piece_batch {
    rf_geomod_piece_bank *geometry;rf_physics_body *bodies;
    uint32_t count,bytes,peak_bytes;
};
void rf_geomod_piece_batch_close(rf_geomod_piece_batch **owner)
{
    uint32_t i;rf_geomod_piece_batch *batch;
    if(!owner || !*owner)return;batch=*owner;
    for(i=0;i<batch->count;i++)rf_physics_body_close(batch->bodies+i);
    rf_geomod_piece_bank_close(&batch->geometry);free(batch);*owner=NULL;
}
uint32_t rf_geomod_piece_batch_count(const rf_geomod_piece_batch *batch)
{return batch?batch->count:0;}
uint32_t rf_geomod_piece_batch_bytes(const rf_geomod_piece_batch *batch)
{return batch?batch->bytes:0;}
uint32_t rf_geomod_piece_batch_peak_bytes(const rf_geomod_piece_batch *batch)
{return batch?batch->peak_bytes:0;}
int rf_geomod_piece_batch_get(rf_geomod_piece_batch *batch,uint32_t index,
    rf_geomod_owned_piece *piece,rf_physics_body **body)
{
    rf_geomod_owned_piece value;int status;
    if(!batch || !piece || !body || index>=batch->count)return RF_RANGE;
    status=rf_geomod_piece_bank_get(batch->geometry,index,&value);if(status)return status;
    *piece=value;*body=batch->bodies+index;return RF_OK;
}
int rf_geomod_piece_batch_open(const rf_geomod_mesh_view *source,const rf_collision_face_filter *filters,
    const rf_collision_face_filter *generated,uint32_t material,float density,float elasticity,float friction,
    rf_random_state *random,uint32_t budget,rf_geomod_piece_batch **out)
{
    rf_geomod_piece_bank *geometry=NULL;rf_geomod_piece_batch *batch=NULL;
    rf_geomod_subdivision_stats stats;rf_random_state next;uint32_t count,i;uint64_t owner_bytes,resident;int status;
    if(!out || *out || !random || !isfinite(elasticity) || !isfinite(friction))return RF_RANGE;
    next=*random;
    status=rf_geomod_piece_subdivide(source,filters,generated,material,density,&next,budget,&geometry,&stats);if(status)return status;
    count=rf_geomod_piece_bank_count(geometry);owner_bytes=sizeof(*batch)+(uint64_t)count*sizeof(rf_physics_body);
    resident=owner_bytes+rf_geomod_piece_bank_bytes(geometry);
    if(resident>budget){status=RF_RANGE;goto failed;}
    batch=calloc(1,(size_t)owner_bytes);if(!batch){status=RF_IO;goto failed;}
    batch->geometry=geometry;geometry=NULL;batch->bodies=(rf_physics_body *)(batch+1);
    batch->bytes=(uint32_t)resident;batch->peak_bytes=stats.peak_bytes;
    for(i=0;i<count;i++) {
        rf_geomod_owned_piece piece;
        status=rf_geomod_piece_bank_get(batch->geometry,i,&piece);if(status)goto failed;
        /* The body record is already in the fixed owner allocation. Only its
         * separately allocated spheres increase the concurrent resident bytes. */
        status=rf_geomod_piece_body_open(&piece,elasticity,friction,budget-batch->bytes+sizeof(rf_physics_body),batch->bodies+i);
        if(status)goto failed;
        batch->count++;batch->bytes+=batch->bodies[i].allocated_bytes-sizeof(rf_physics_body);
    }
    if(batch->bytes>batch->peak_bytes)batch->peak_bytes=batch->bytes;
    *random=next;*out=batch;return RF_OK;
failed:
    rf_geomod_piece_bank_close(&geometry);rf_geomod_piece_batch_close(&batch);return status;
}

int rf_geomod_piece_batch_sweep(const rf_geomod_piece_batch *batch,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_piece_hit *result,uint32_t *matched)
{
    rf_geomod_piece_hit best={0};uint32_t found=0,i,k;float nearest=limit;int status;
    if(!batch || !start || !delta || !result || !matched || !isfinite(radius) || radius<0 ||
        !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(start[k]) || !isfinite(delta[k]))return RF_RANGE;
    for(i=0;i<batch->count;i++) {
        rf_geomod_owned_piece piece;rf_collision_sweep_tree_hit local;uint32_t hit;
        const rf_physics_body_state *body=&batch->bodies[i].state;
        status=rf_geomod_piece_bank_get(batch->geometry,i,&piece);if(status)return status;
        status=rf_collision_flat_faces(piece.collision,piece.mesh.face_count,flags&~4u,start,delta,
            body->position,(const float (*)[3])body->orientation,radius,nearest,&local,&hit);
        if(status)return status;
        if(!hit || (found && local.hit.fraction>=nearest))continue;
        status=rf_collision_contact_world(&local.hit,body->position,(const float (*)[3])body->orientation,&best.hit);
        if(status)return status;
        best.piece=i;best.face=local.face_index;best.edge=local.edge;
        nearest=local.hit.fraction;found=1;
    }
    if(found)*result=best;
    *matched=found;return RF_OK;
}

typedef struct piece_registry_entry {
    rf_geomod_piece_batch *batch;uint32_t prefix,ordinal,before,after;
} piece_registry_entry;
struct rf_geomod_piece_registry {
    piece_registry_entry active[16],pending[16];uint32_t count,staged,begun,replace;
    uint32_t seed,budget,bytes,material,last_prefix,last_ordinal;rf_random_state random;
    float density,elasticity,friction;rf_collision_face_filter generated;
};
int rf_geomod_piece_registry_open(const rf_collision_face_filter *generated,uint32_t material,
    float density,float elasticity,float friction,uint32_t seed,uint32_t budget,rf_geomod_piece_registry **out)
{
    rf_geomod_piece_registry *r;
    if(!out || *out || !generated || budget<sizeof(*r) || material==UINT32_MAX ||
       !isfinite(density) || density<0 || !isfinite(elasticity) || !isfinite(friction))return RF_RANGE;
    r=calloc(1,sizeof(*r));if(!r)return RF_IO;
    r->generated=*generated;r->material=material;r->density=density;r->elasticity=elasticity;r->friction=friction;
    r->seed=seed;r->budget=budget;r->bytes=sizeof(*r);*out=r;return RF_OK;
}
void rf_geomod_piece_registry_abort(rf_geomod_piece_registry *r)
{
    uint32_t i;if(!r)return;
    for(i=0;i<r->staged;i++){r->bytes-=rf_geomod_piece_batch_bytes(r->pending[i].batch);rf_geomod_piece_batch_close(&r->pending[i].batch);}
    memset(r->pending,0,sizeof(r->pending));r->staged=r->begun=r->replace=0;
}
void rf_geomod_piece_registry_close(rf_geomod_piece_registry **owner)
{
    rf_geomod_piece_registry *r;uint32_t i;if(!owner || !*owner)return;r=*owner;
    rf_geomod_piece_registry_abort(r);
    for(i=0;i<r->count;i++)rf_geomod_piece_batch_close(&r->active[i].batch);
    free(r);*owner=NULL;
}
int rf_geomod_piece_registry_rewind(rf_geomod_piece_registry *r)
{if(!r || !r->begun)return RF_RANGE;r->random.value=r->seed;r->last_prefix=r->last_ordinal=0;return RF_OK;}
int rf_geomod_piece_registry_begin(rf_geomod_piece_registry *r,uint32_t replace)
{
    if(!r || r->begun || replace>1)return RF_RANGE;
    r->begun=1;r->replace=replace;return rf_geomod_piece_registry_rewind(r);
}
int rf_geomod_piece_registry_emit(const rf_geomod_mesh_view *mesh,const uint32_t *map,
    const rf_collision_face_filter *filters,uint32_t source_count,uint32_t prefix,uint32_t ordinal,void *opaque)
{
    rf_geomod_piece_registry *r=opaque;piece_registry_entry *entry;
    rf_collision_face_filter mapped[32];uint32_t i,pass;int status;
    if(!r || !mesh || !map || !filters || !prefix)return RF_RANGE;
    /* Read-only history validation can revisit already committed pieces without
     * opening an edit or touching body/RNG state. New identities still reject. */
    if(!r->begun) {
        for(i=0;i<r->count;i++)if(r->active[i].prefix==prefix && r->active[i].ordinal==ordinal)return RF_OK;
        return RF_NOT_FOUND;
    }
    if(prefix<r->last_prefix || (prefix==r->last_prefix && ordinal<=r->last_ordinal))return RF_RANGE;
    for(pass=0;pass<2;pass++) {
        piece_registry_entry *entries=pass?r->pending:r->active;
        uint32_t count=pass?r->staged:(r->replace?0:r->count);
        for(i=0;i<count;i++)if(entries[i].prefix==prefix && entries[i].ordinal==ordinal) {
            if(entries[i].before!=r->random.value)return RF_FORMAT;
            r->random.value=entries[i].after;r->last_prefix=prefix;r->last_ordinal=ordinal;return RF_OK;
        }
    }
    if(mesh->face_count>32 || r->staged+(r->replace?0:r->count)>=16)return RF_RANGE;
    for(i=0;i<mesh->face_count;i++){if(map[i]>=source_count)return RF_FORMAT;mapped[i]=filters[map[i]];}
    entry=r->pending+r->staged;entry->before=r->random.value;
    status=rf_geomod_piece_batch_open(mesh,mapped,&r->generated,r->material,r->density,r->elasticity,r->friction,
        &r->random,r->budget-r->bytes,&entry->batch);if(status)return status;
    entry->prefix=prefix;entry->ordinal=ordinal;entry->after=r->random.value;
    r->bytes+=rf_geomod_piece_batch_bytes(entry->batch);r->staged++;r->last_prefix=prefix;r->last_ordinal=ordinal;return RF_OK;
}
void rf_geomod_piece_registry_commit(rf_geomod_piece_registry *r)
{
    uint32_t i;if(!r || !r->begun)return;
    if(r->replace) {
        for(i=0;i<r->count;i++){r->bytes-=rf_geomod_piece_batch_bytes(r->active[i].batch);rf_geomod_piece_batch_close(&r->active[i].batch);}
        memset(r->active,0,sizeof(r->active));r->count=0;
    }
    memcpy(r->active+r->count,r->pending,r->staged*sizeof(*r->pending));r->count+=r->staged;
    memset(r->pending,0,sizeof(r->pending));r->staged=r->begun=r->replace=0;
}
uint32_t rf_geomod_piece_registry_count(const rf_geomod_piece_registry *r){return r?r->count:0;}
uint32_t rf_geomod_piece_registry_bytes(const rf_geomod_piece_registry *r){return r?r->bytes:0;}
int rf_geomod_piece_registry_get(rf_geomod_piece_registry *r,uint32_t i,rf_geomod_piece_batch **out)
{if(!r || !out || i>=r->count)return RF_RANGE;*out=r->active[i].batch;return RF_OK;}

#define PIECE_STATE_HEADER 16u
#define PIECE_STATE_RECORD 320u
static uint32_t piece_state_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void piece_state_store(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
/* Explicit fields, independent of host struct padding and pointer width. */
static void piece_state_codec(rf_physics_body_state *s,unsigned char *p,uint32_t read)
{
    float *fields[]={s->coefficients,&s->mass,s->local_tensor,s->world_tensor,s->position,s->next_position,
        s->orientation,s->next_orientation,s->velocity,s->vector_c8,s->mass_vector_d4,s->vector_e0,s->vector_ec,
        &s->bounds.radius,s->bounds.minimum,s->bounds.maximum,s->vector_138,&s->scalar_144};
    const uint32_t counts[]={3,1,9,9,3,3,9,9,3,3,3,3,3,1,3,3,3,1};uint32_t i,j,w;
    uint32_t words[5];
    for(i=0;i<18;i++)for(j=0;j<counts[i];j++,p+=4) {
        if(read){w=piece_state_word(p);memcpy(fields[i]+j,&w,4);}
        else {memcpy(&w,fields[i]+j,4);piece_state_store(p,w);}
    }
    if(read) {
        for(i=0;i<5;i++)words[i]=piece_state_word(p+4*i);
        s->flags=words[0];s->state_124=words[1];memcpy(&s->reference_15c,words+2,4);
        s->word_164=words[3];s->word_168=words[4];
    } else {
        words[0]=s->flags;words[1]=s->state_124;memcpy(words+2,&s->reference_15c,4);
        words[3]=s->word_164;words[4]=s->word_168;
        for(i=0;i<5;i++)piece_state_store(p+4*i,words[i]);
    }
}
static int piece_state_valid(const rf_physics_body_state *s,const rf_physics_body_state *birth)
{
    unsigned char encoded[308];rf_physics_body_state copy=*s;uint32_t i,j,k,w;float f,tensor[9];
    piece_state_codec(&copy,encoded,0);
    for(i=0;i<72;i++){w=piece_state_word(encoded+4*i);memcpy(&f,&w,4);if(!isfinite(f))return RF_FORMAT;}
    if(s->mass<=0 || s->bounds.radius<0 || s->coefficients[1]<0 || s->coefficients[2]<0 ||
       s->scalar_144<0 || s->scalar_144>1 || s->mass!=birth->mass || memcmp(s->local_tensor,birth->local_tensor,36) ||
       s->bounds.radius!=birth->bounds.radius || s->coefficients[1]!=birth->coefficients[1] ||
       s->coefficients[2]!=birth->coefficients[2] || s->coefficients[0]<0 || (s->flags&0x40000100u) ||
       (s->flags&0x4000u) || s->reference_15c!=birth->reference_15c || s->word_168!=birth->word_168)return RF_FORMAT;
    if(rf_physics_tensor_world(s->local_tensor,s->orientation,tensor))return RF_FORMAT;
    for(i=0;i<9;i++)if(tensor[i]!=s->world_tensor[i])return RF_FORMAT;
    for(i=0;i<3;i++)if(s->bounds.minimum[i]>s->bounds.maximum[i])return RF_FORMAT;
    for(k=0;k<2;k++) {
        const float *m=k?s->next_orientation:s->orientation;
        for(i=0;i<3;i++)for(j=0;j<3;j++) {
            double dot=(double)m[3*i]*m[3*j]+(double)m[3*i+1]*m[3*j+1]+(double)m[3*i+2]*m[3*j+2];
            if(fabs(dot-(i==j?1:0))>.01)return RF_FORMAT;
        }
        if((double)m[0]*(m[4]*m[8]-m[5]*m[7])-(double)m[1]*(m[3]*m[8]-m[5]*m[6])+
           (double)m[2]*(m[3]*m[7]-m[4]*m[6])<.99)return RF_FORMAT;
    }
    return RF_OK;
}
int rf_geomod_piece_registry_state_size(const rf_geomod_piece_registry *r,uint32_t *bytes)
{
    uint64_t n=PIECE_STATE_HEADER;uint32_t i;
    if(!r || !bytes || r->begun)return RF_RANGE;
    for(i=0;i<r->count;i++)n+=(uint64_t)r->active[i].batch->count*PIECE_STATE_RECORD;
    if(n>UINT32_MAX)return RF_RANGE;*bytes=(uint32_t)n;return RF_OK;
}
int rf_geomod_piece_registry_state_encode(const rf_geomod_piece_registry *r,void *output,uint32_t bytes)
{
    unsigned char *p=output;uint32_t size,i,j;int status=rf_geomod_piece_registry_state_size(r,&size);
    if(status)return status;if(!output || bytes!=size)return RF_RANGE;
    for(i=0;i<r->count;i++)for(j=0;j<r->active[i].batch->count;j++) {
        const rf_physics_body_state *s=&r->active[i].batch->bodies[j].state;
        status=piece_state_valid(s,s);if(status)return status;
    }
    memcpy(p,"RFPB",4);piece_state_store(p+4,1);piece_state_store(p+8,size);
    piece_state_store(p+12,(size-PIECE_STATE_HEADER)/PIECE_STATE_RECORD);p+=PIECE_STATE_HEADER;
    for(i=0;i<r->count;i++)for(j=0;j<r->active[i].batch->count;j++,p+=PIECE_STATE_RECORD) {
        rf_physics_body_state s=r->active[i].batch->bodies[j].state;
        piece_state_store(p,r->active[i].prefix);piece_state_store(p+4,r->active[i].ordinal);piece_state_store(p+8,j);
        piece_state_codec(&s,p+12,0);
    }
    return RF_OK;
}
int rf_geomod_piece_registry_state_decode(rf_geomod_piece_registry *r,const void *input,uint32_t bytes)
{
    const unsigned char *start=input,*p;uint32_t size,i,j,pass;int status=rf_geomod_piece_registry_state_size(r,&size);
    if(status)return status;if(!input || bytes!=size || bytes<PIECE_STATE_HEADER)return RF_FORMAT;
    if(memcmp(start,"RFPB",4) || piece_state_word(start+4)!=1 || piece_state_word(start+8)!=size ||
       piece_state_word(start+12)!=(size-PIECE_STATE_HEADER)/PIECE_STATE_RECORD)return RF_FORMAT;
    for(pass=0;pass<2;pass++) {
        p=start+PIECE_STATE_HEADER;
        for(i=0;i<r->count;i++)for(j=0;j<r->active[i].batch->count;j++,p+=PIECE_STATE_RECORD) {
            rf_physics_body_state state={0},*target=&r->active[i].batch->bodies[j].state;
            if(piece_state_word(p)!=r->active[i].prefix || piece_state_word(p+4)!=r->active[i].ordinal || piece_state_word(p+8)!=j)return RF_FORMAT;
            piece_state_codec(&state,(unsigned char *)p+12,1);
            if(!pass){status=piece_state_valid(&state,target);if(status)return status;}
            else *target=state;
        }
    }
    return RF_OK;
}

int rf_geomod_piece_registry_sweep(const rf_geomod_piece_registry *r,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_registry_hit *out,uint32_t *matched)
{
    rf_geomod_registry_hit best={0};uint32_t i,k,found=0;float nearest=limit;int status;
    if(!start || !delta || !out || !matched || !isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(start[k]) || !isfinite(delta[k]))return RF_RANGE;
    if(r && r->begun)return RF_RANGE;
    for(i=0;r && i<r->count;i++) {
        rf_geomod_piece_hit hit;uint32_t candidate;
        status=rf_geomod_piece_batch_sweep(r->active[i].batch,flags,start,delta,radius,nearest,&hit,&candidate);if(status)return status;
        if(!candidate || (found && hit.hit.fraction>=nearest))continue;
        best.piece=hit;best.batch=i;nearest=hit.hit.fraction;found=1;
    }
    if(found)*out=best;*matched=found;return RF_OK;
}

typedef struct piece_body_query_context {
    const rf_geomod_piece_registry *registry;uint32_t material;
    rf_geomod_registry_hit selected;uint32_t sphere;float velocity[3];
} piece_body_query_context;
static int piece_body_query(void *opaque,const rf_collision_body_request *request,
    rf_collision_body_candidate *candidate,uint32_t *matched)
{
    piece_body_query_context *context=opaque;rf_geomod_registry_hit hit;
    const rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;int status;
    status=rf_geomod_piece_registry_sweep(context->registry,request->flags,request->start,request->delta,
        request->radius,request->limit,&hit,matched);if(status || !*matched)return status;
    batch=context->registry->active[hit.batch].batch;
    status=rf_geomod_piece_bank_get(batch->geometry,hit.piece.piece,&piece);if(status)return status;
    candidate->hit=hit.piece.hit;candidate->material=context->material;
    candidate->texture=piece.mesh.faces[hit.piece.face].material;
    candidate->face_flags=piece.filters[hit.piece.face].face_flags;candidate->face_token=UINT32_MAX;
    context->selected=hit;context->sphere=request->sphere;
    memcpy(context->velocity,batch->bodies[hit.piece.piece].state.velocity,12);return RF_OK;
}
int rf_geomod_piece_registry_body_sweep(const rf_geomod_piece_registry *registry,
    const rf_collision_body_query *query,uint32_t material,rf_geomod_registry_body_hit *out,uint32_t *matched)
{
    piece_body_query_context context={0};rf_geomod_registry_body_hit result={0};uint32_t found;int status;
    if(!out || !matched || (registry && registry->begun))return RF_RANGE;
    context.registry=registry;context.material=material;
    status=rf_collision_body_sweep(query,NULL,0,piece_body_query,&context,&result.contact,&found);if(status)return status;
    if(found) {
        result.batch=context.selected.batch;result.piece=context.selected.piece.piece;
        result.face=context.selected.piece.face;result.sphere=context.sphere;
        memcpy(result.contact.velocity,context.velocity,12);*out=result;
    }
    *matched=found;return RF_OK;
}
