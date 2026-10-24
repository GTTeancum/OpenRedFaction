#include "rf/geomod_retained_material_digest.h"
#include "rf/geomod_limits.h"
#include <math.h>
#include <string.h>
/* SHA-256 shared with the other GeoMod digests; byte-identical output. */
#include "geomod_identity_sha.h"
static int map_validate(const rf_geomod_retained_material_map *m)
{
    uint32_t j,axis=0,dims[2];float span[2],density[2]={4,4},adjusted[2];double length=0;int status;
    if(m->width<3 || m->width>64 || m->height<3 || m->height>64 ||
        m->x>512-m->width || m->y>512-m->height)return RF_FORMAT;
    for(j=0;j<4;j++)if(!isfinite(m->plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++) {
        if(!isfinite(m->minimum[j]) || !isfinite(m->maximum[j]) || m->minimum[j]>m->maximum[j])return RF_FORMAT;
        length+=(double)m->plane[j]*m->plane[j];
    }
    if(fabs(length-1)>1e-4)return RF_FORMAT;
    for(j=1;j<3;j++)if(fabsf(m->plane[j])>fabsf(m->plane[axis]))axis=j;
    for(j=0;j<2;j++) {
        uint32_t a=(axis+j+1)%3;double scale;float expected_scale,expected_offset;
        if(m->projection.axes[j]!=a)return RF_FORMAT;
        span[j]=m->maximum[a]-m->minimum[a];if(!isfinite(span[j]) || !(span[j]>0))return RF_FORMAT;
        scale=((j?m->height:m->width)-2)/(double)span[j];
        expected_scale=(float)(scale/512);expected_offset=(float)(((j?m->y:m->x)+1-m->minimum[a]*scale)/512);
        if(!isfinite(m->projection.scale[j]) || m->projection.scale[j]<=0 || !isfinite(m->projection.offset[j]) ||
            m->projection.scale[j]!=expected_scale || m->projection.offset[j]!=expected_offset)return RF_FORMAT;
    }
    status=rf_geomod_lightmap_size(span,density,0,dims,adjusted);if(status)return status;
    return dims[0]==m->width && dims[1]==m->height?RF_OK:RF_FORMAT;
}
static int retained_digest(const rf_geomod_retained_material_input *v,
    const rf_geomod_retained_material_source *sources,uint32_t source_count,unsigned char out[32])
{
    identity_sha h;unsigned char digest[32],rgb[64*3],packed[64*2];rf_random_state chain={1};
    uint32_t i,j,x=0,y=0,row=0;int status;
    if(!v || !out || (!source_count && !v->source_identity) || !v->substrate_identity || v->map_count>RF_GEOMOD_LIGHTMAP_LIMIT || v->face_count>RF_GEOMOD_PUBLICATION_FACES ||
        (v->map_count && !v->maps) || (v->face_count && (!v->origins || !v->face_maps)))return RF_RANGE;
    if(source_count) {
        uint32_t cuts=0;
        if(!sources || source_count>4)return RF_RANGE;
        for(i=0;i<source_count;i++) {
            uint32_t nonzero=0;
            if(sources[i].uid==UINT32_MAX || !sources[i].identity || sources[i].cuts>RF_GEOMOD_CUT_LIMIT || sources[i].cuts>v->serial)return RF_FORMAT;
            for(j=0;j<i;j++)if(sources[j].uid==sources[i].uid)return RF_FORMAT;
            for(j=0;j<32;j++)nonzero|=sources[i].identity[j];if(!nonzero)return RF_FORMAT;
            cuts+=sources[i].cuts;
        }
        if(v->cuts!=cuts || v->owner!=sources[0].uid)return RF_FORMAT;
    }
    if(v->authored_material_count>128 || (v->authored_material_count && !v->authored_material_tokens))return RF_RANGE;
    if(v->material_policy==1 && v->authored_material_count)return RF_FORMAT;
    for(i=0;i<v->authored_material_count;i++) {
        uint32_t token=v->authored_material_tokens[i];if(!token || token==UINT32_MAX)return RF_FORMAT;
        for(j=0;j<i;j++)if(v->authored_material_tokens[j]==token)return RF_FORMAT;
    }
    if((v->material_policy!=1 && v->material_policy!=2) || v->owner==UINT32_MAX || v->serial==UINT32_MAX || (!source_count && (v->cuts>RF_GEOMOD_CUT_LIMIT || v->cuts>v->serial)) ||
        v->baked!=v->map_count || v->sample || v->owner_cuts!=v->cuts)return RF_FORMAT;
    if(v->owner_generation!=v->serial && (v->owner_generation || v->cuts || v->map_count))return RF_FORMAT;
    if(!v->cuts && v->map_count)return RF_FORMAT; /* Authored reset clears its journal. */
    if(!v->map_count && !v->cuts) {
        if(v->random>1 || (v->owner_generation && !v->random))return RF_FORMAT;
        chain.value=v->random;
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFRM",4);identity_sha_word(&h,(source_count?2:1)+(v->material_policy==2?2:0));identity_sha_word(&h,v->material_policy);
    if(source_count) {
        identity_sha_word(&h,source_count);
        for(i=0;i<source_count;i++) {
            identity_sha_word(&h,sources[i].uid);identity_sha_word(&h,sources[i].cuts);identity_sha_add(&h,sources[i].identity,32);
        }
    } else identity_sha_add(&h,v->source_identity,32);
    identity_sha_add(&h,v->substrate_identity,32);
    identity_sha_word(&h,v->owner);identity_sha_word(&h,v->serial);identity_sha_word(&h,v->cuts);
    identity_sha_word(&h,v->owner_generation);identity_sha_word(&h,v->owner_cuts);
    identity_sha_word(&h,v->map_count);identity_sha_word(&h,v->baked);identity_sha_word(&h,v->sample);
    identity_sha_word(&h,v->random);identity_sha_word(&h,v->x);identity_sha_word(&h,v->y);identity_sha_word(&h,v->row);
    identity_sha_word(&h,v->face_count);
    for(i=0;i<v->map_count;i++) {
        const rf_geomod_retained_material_map *m=v->maps+i;uint32_t remaining;
        status=map_validate(m);if(status)return status;
        if(m->material_token) {
            if(v->material_policy!=2)return RF_FORMAT;
            for(j=0;j<v->authored_material_count;j++)if(v->authored_material_tokens[j]==m->material_token)break;
            if(j==v->authored_material_count)return RF_FORMAT;
        }
        if(x+m->width>512){y+=row;x=row=0;}
        if(y+m->height>512 || m->x!=x || m->y!=y || m->base_seed!=chain.value)return RF_FORMAT;
        x+=m->width;if(m->height>row)row=m->height;
        for(j=0;j<4;j++)identity_sha_float(&h,m->plane[j]);
        for(j=0;j<3;j++)identity_sha_float(&h,m->minimum[j]);
        for(j=0;j<3;j++)identity_sha_float(&h,m->maximum[j]);
        identity_sha_word(&h,m->material_token);identity_sha_word(&h,m->x);identity_sha_word(&h,m->y);
        identity_sha_word(&h,m->width);identity_sha_word(&h,m->height);identity_sha_word(&h,m->base_seed);
        for(j=0;j<2;j++)identity_sha_word(&h,m->projection.axes[j]);
        for(j=0;j<2;j++)identity_sha_float(&h,m->projection.scale[j]);
        for(j=0;j<2;j++)identity_sha_float(&h,m->projection.offset[j]);
        /* Hash base pixels in local row-major order, never dynamic atlas bytes.
         * Each pixel advances one original RNG draw, including unused maps. */
        remaining=m->width*m->height;identity_sha_word(&h,remaining*2);
        while(remaining) {
            uint32_t n=remaining>64?64:remaining;
            status=rf_geomod_light_noise(rgb,sizeof(rgb),n*3,n,1,&chain);if(status)return status;
            status=rf_lightmap_pack_1555(rgb,n*3,n,1,0,packed,n*2,sizeof(packed));if(status)return status;
            identity_sha_add(&h,packed,n*2);remaining-=n;
        }
    }
    if(x!=v->x || y!=v->y || row!=v->row || chain.value!=v->random)return RF_FORMAT;
    for(i=0;i<v->face_count;i++) {
        const rf_geomod_publication_origin *o=v->origins+i;uint32_t map=v->face_maps[i];
        if(o->owner==UINT32_MAX || o->kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER) {
            if(source_count) {
                for(j=0;j<source_count;j++)if(sources[j].uid==o->owner && sources[j].cuts)break;
                if(j==source_count)return RF_FORMAT;
            } else if(o->owner!=v->owner)return RF_FORMAT;
            if(o->source_face!=UINT32_MAX || map>=v->map_count || v->maps[map].material_token)return RF_FORMAT;
        } else if(v->material_policy==2 && o->kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && o->reference==UINT32_MAX) {
            if(!v->cuts || o->source_face==UINT32_MAX || map>=v->map_count || !v->maps[map].material_token)return RF_FORMAT;
        } else if(o->source_face==UINT32_MAX || map!=65535)return RF_FORMAT;
        identity_sha_word(&h,o->kind);identity_sha_word(&h,o->owner);identity_sha_word(&h,o->source_face);identity_sha_word(&h,map);
    }
    identity_sha_end(&h,digest);memcpy(out,digest,32);return RF_OK;
}

int rf_geomod_retained_material_digest(const rf_geomod_retained_material_input *v,unsigned char out[32])
{return retained_digest(v,NULL,0,out);}
int rf_geomod_retained_material_collection_digest(const rf_geomod_retained_material_input *v,
    const rf_geomod_retained_material_source *sources,uint32_t count,unsigned char out[32])
{
    if(!count || count>4 || !sources)return RF_RANGE;
    return retained_digest(v,sources,count,out);
}
