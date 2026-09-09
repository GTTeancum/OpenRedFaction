#include "rf/model.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

int rf_model_triangle_facing(const float a[3],const float b[3],const float c[3],uint16_t flags,
    uint32_t perspective,const float camera[3],const float forward[3],uint32_t *accepted)
{
    float ab[3],bc[3],normal[3],direction[3];double dot;unsigned i;
    if(!accepted || !a || !b || !c || !camera || !forward)return RF_RANGE;
    if(flags&0x20) {*accepted=1;return RF_OK;}
    for(i=0;i<3;++i) {ab[i]=b[i]-a[i];bc[i]=c[i]-b[i];}
    normal[0]=(float)((double)ab[1]*bc[2]-(double)ab[2]*bc[1]);
    normal[1]=(float)((double)ab[2]*bc[0]-(double)ab[0]*bc[2]);
    normal[2]=(float)((double)ab[0]*bc[1]-(double)ab[1]*bc[0]);
    for(i=0;i<3;++i)direction[i]=perspective?camera[i]-a[i]:forward[i];
    dot=((double)direction[0]*normal[0]+(double)direction[1]*normal[1])+(double)direction[2]*normal[2];
    *accepted=perspective?(dot>0):!(dot>0);return RF_OK;
}

int rf_model_project_vertex(const float world[3],const rf_model_projection *view,
    rf_model_render_cache *cache,float clip_position[3],uint8_t vertex[40],uint32_t *visible)
{
    float delta[3],position[3],depth,biased;double reciprocal;uint32_t bits;uint8_t clip=0;unsigned i;
    const float *m;
    if(!world || !view || !cache || !clip_position || !vertex || !visible)return RF_RANGE;
    m=view->rotation;
    for(i=0;i<3;++i)delta[i]=world[i]-view->camera[i];
    position[0]=(float)(((double)m[1]*delta[1]+(double)m[2]*delta[2])+(double)m[0]*delta[0]);
    position[1]=(float)(((double)m[4]*delta[1]+(double)m[3]*delta[0])+(double)m[5]*delta[2]);
    position[2]=(float)(((double)m[7]*delta[1]+(double)m[6]*delta[0])+(double)m[8]*delta[2]);
    if(!view->perspective)position[2]=view->fixed_depth;
    if(view->compute_clip) {
        if(view->clipping) {
            if(position[0]>position[2])clip|=8;
            if(position[1]>position[2])clip|=32;
            if(-position[2]>position[0])clip|=4;
            if(-position[2]>position[1])clip|=16;
            if(view->perspective) {
                if(!(position[2]>0))clip|=128;
                if(view->far_clip && position[2]>view->far_depth)clip|=2;
            }
        }
        memcpy(clip_position,position,sizeof(position));
    }
    depth=(float)(255.0-(double)view->depth_factor*position[2]);
    if(!(depth>=0))depth=0;else if(depth>255)depth=255;
    biased=depth+12582912.0f;memcpy(&bits,&biased,4);cache->depth=(uint8_t)bits;vertex[23]=cache->depth;
    reciprocal=1.0/(double)position[2];
    cache->projected[2]=(float)reciprocal;
    cache->projected[0]=(float)((reciprocal*view->screen[0])*position[0]+view->screen[2]);
    cache->projected[1]=(float)((reciprocal*position[1])*view->screen[1]+view->screen[3]);
    *visible=1;
    if(view->screen_clip && !(cache->projected[0]>view->bounds[0] && cache->projected[0]<view->bounds[2] &&
        cache->projected[1]>view->bounds[1] && cache->projected[1]<view->bounds[3] && cache->projected[2]>=0)) {
        clip=1;*visible=0;
    }
    cache->clip=clip;return RF_OK;
}

static void emit_render_vertex(const rf_model_render_cache *source,const rf_model_render_output *output,
    const float uv[2],uint8_t vertex[40])
{
    const uint8_t *rgb;float value;
    memcpy(vertex,source->projected,8);
    value=output->depth_scale*source->projected[2];memcpy(vertex+8,&value,4);
    value=output->reciprocal_scale*source->projected[2];memcpy(vertex+12,&value,4);
    rgb=output->lighting?source->rgb:output->rgb;
    vertex[16]=rgb[2];vertex[17]=rgb[1];vertex[18]=rgb[0];vertex[19]=output->alpha;
    vertex[23]=source->depth;memcpy(vertex+24,uv,8);
}

int rf_model_render_reuse_vertex(rf_model_render_cache *cache,uint32_t count,uint32_t index,int32_t distance,
    const rf_model_render_output *output,const float uv[2],uint8_t vertex[40])
{
    const rf_model_render_cache *source;
    if(!cache || !output || !uv || !vertex || index>=count || distance<=0 || (uint32_t)distance>index)return RF_RANGE;
    source=cache+index-(uint32_t)distance;
    memcpy(cache[index].world,source->world,12);cache[index].clip=source->clip;
    if(!source->clip)emit_render_vertex(source,output,uv,vertex);
    return RF_OK;
}

int rf_model_finish_render_vertex(rf_model_render_cache *cache,float second[3],
    const rf_model_render_output *output,const float lights[3][6],const float ambient[3],
    const float uv[2],uint8_t vertex[40])
{
    double inverse;unsigned i;int status;
    if(!cache || !second || !output || !lights || !ambient || !uv || !vertex)return RF_RANGE;
    inverse=1.0/sqrt(((double)second[0]*second[0]+(double)second[1]*second[1])+(double)second[2]*second[2]);
    for(i=0;i<3;++i)second[i]=(float)(inverse*second[i]);
    if(output->lighting) {
        status=rf_model_vertex_lighting(second,lights,ambient,cache->rgb);if(status)return status;
    }
    emit_render_vertex(cache,output,uv,vertex);return RF_OK;
}

int rf_model_render_vertex_lighting(const float vector[3],const float lights[3][6],const float ambient[3],
    float normalized[3],uint8_t rgb[3])
{
    float value[3];double squared,inverse;unsigned i;int status;
    if(!vector || !lights || !ambient || !normalized || !rgb)return RF_RANGE;
    squared=((double)vector[0]*vector[0]+(double)vector[1]*vector[1])+(double)vector[2]*vector[2];
    inverse=1.0/sqrt(squared);
    for(i=0;i<3;++i)value[i]=(float)(inverse*vector[i]);
    status=rf_model_vertex_lighting(value,lights,ambient,rgb);if(status)return status;
    memcpy(normalized,value,sizeof(value));return RF_OK;
}

int rf_model_lod_metric(uint32_t mode,const float position[3],const float camera[3],
    float numerator,float denominator,double *out)
{
    float delta[3];unsigned i;double squared;
    if(!out)return RF_RANGE;
    if(mode!=0x66) {*out=0;return RF_OK;}
    if(!position || !camera)return RF_RANGE;
    for(i=0;i<3;++i)delta[i]=position[i]-camera[i];
    squared=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
    *out=(sqrt(squared)*numerator)/denominator;return RF_OK;
}

int rf_model_select_lod(const float *thresholds,uint32_t count,uint32_t flags,
    int alternate,int32_t minimum,int scaled,int animated,double metric,uint32_t *out)
{
    int32_t index,selected=0;
    if(!thresholds || !out || !count || count>3 || minimum<0)return RF_RANGE;
    if(flags&9)selected=(int32_t)count-1;
    else if(!alternate && count>1) {
        selected=minimum<(int32_t)count-1?minimum:(int32_t)count-1;
        if(scaled && animated)metric*=2.5;
        for(index=(int32_t)count-1;index>=selected;--index)
            if(metric>=thresholds[index]) {selected=index;break;}
    }
    *out=(uint32_t)selected;return RF_OK;
}

int rf_model_prepare_skinning(const float (*stored)[12],const float (*pose)[12],uint32_t count,
    uint16_t generation,float (*prepared)[12],uint16_t *generations,uint32_t capacity)
{
    uint32_t i;int status;
    if(count>256 || count>capacity || (count && (!stored || !pose || !prepared || !generations)))return RF_RANGE;
    for(i=0;i<count;++i)if(generations[i]!=generation) {
        status=rf_model_compose_transform(stored[i],pose[i],prepared[i]);if(status)return status;
        generations[i]=generation;
    }
    return RF_OK;
}

int rf_model_lighting_setup(const rf_model_lighting_input *input,const rf_model_local_light *lights,
    const float (*colors)[3],uint32_t count,rf_model_lighting *result)
{
    static const uint32_t directions[3][3]={{0x3ee96429,0x3f11de8b,0x3f2f0b72},{0x3eac78ea,0x3ed79746,0x3f579735},{0xbee38e37,0xbe638e37,0xbee38e37}};
    rf_model_lighting out={0};rf_model_light_choice choice;float luma,gain;uint32_t i,j;int status;
    if(!input || !result || ((!lights || !colors) && count))return RF_RANGE;
    luma=(float)(((double)input->color[2]*0.33f+(double)input->color[1]*0.66f)+(double)input->color[0]*0.33f);
    gain=input->alternate?(float)((double)input->gain*1.3f):1.3f;
    memcpy(out.lights[0],directions[input->alternate?1:0],12);memcpy(out.lights[1],directions[2],12);
    for(i=0;i<2;++i)if(i || !input->alternate) {
        float v[3];memcpy(v,out.lights[i],12);
        for(j=0;j<3;++j)out.lights[i][j]=(float)(((double)v[2]*input->model_rotation[6+j]+(double)v[1]*input->model_rotation[3+j])+(double)v[0]*input->model_rotation[j]);
    }
    for(i=0;i<3;++i) {
        float value=gain*input->color[i];if(value<0)value=0;if(value>255)value=255;
        out.lights[0][3+i]=value;out.ambient[i]=input->ambient[i]*255.0f;
        out.lights[1][3+i]=(float)(((double)luma+input->color[i==2?2:1])*0.5*input->gain*0.75);
    }
    if(!input->disable_local && count) {
        status=rf_model_choose_local_light(input->position,lights,count,&choice);if(status)return status;
        if(choice.index>=0) {
            status=rf_model_local_light_direction(choice.delta,input->flags,input->light_rotation,out.lights[2]);if(status)return status;
            status=rf_model_local_light_color(choice.distance_squared,lights[choice.index].radius_squared,colors[choice.index],out.lights[2]+3);if(status)return status;
        }
    }
    *result=out;return RF_OK;
}

static void light_normalize(float v[3])
{
    double length=sqrt((double)v[0]*v[0]+(double)v[1]*v[1]+(double)v[2]*v[2]);uint32_t i;
    if(!(length>0)) { v[0]=1;v[1]=v[2]=0;return; }
    length=1.0/length;for(i=0;i<3;++i)v[i]=(float)(v[i]*length);
}
int rf_model_local_light_direction(const float delta[3],uint32_t flags,const float rotation[9],float result[3])
{
    float v[3],out[3];uint32_t i;
    if(!delta || !rotation || !result)return RF_RANGE;
    memcpy(v,delta,sizeof(v));light_normalize(v);
    if(flags&0x400) { v[1]=0.5f;light_normalize(v); }
    for(i=0;i<3;++i)out[i]=(float)(((double)v[2]*rotation[i*3+2]+(double)v[1]*rotation[i*3+1])+(double)v[0]*rotation[i*3]);
    memcpy(result,out,sizeof(out));return RF_OK;
}

int rf_model_local_light_color(float distance_squared,float radius_squared,const float color[3],float result[3])
{
    double scale;float value[3];uint32_t i;
    if(!color || !result)return RF_RANGE;
    scale=(1.0-sqrt((double)distance_squared/radius_squared))*255.0;
    for(i=0;i<3;++i)value[i]=(float)(scale*color[i]);
    memcpy(result,value,sizeof(value));return RF_OK;
}

int rf_model_choose_local_light(const float position[3],const rf_model_local_light *lights,uint32_t count,rf_model_light_choice *choice)
{
    rf_model_light_choice next={-1,{0,0,0},FLT_MAX};uint32_t i,j;
    if(!position || !choice || (!lights && count) || count>INT32_MAX)return RF_RANGE;
    for(i=0;i<count;++i)if(lights[i].enabled) {
        float delta[3],distance;double squared;
        for(j=0;j<3;++j)delta[j]=lights[i].position[j]-position[j];
        squared=(double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2];distance=(float)squared;
        /* x87 radius comparison accepts unordered; nearest comparison does not. */
        if(!(squared>lights[i].radius_squared) && distance<next.distance_squared) {
            next.index=(int32_t)i;memcpy(next.delta,delta,sizeof(delta));next.distance_squared=distance;
        }
    }
    *choice=next;return RF_OK;
}

int rf_model_vertex_lighting(const float vector[3],const float lights[3][6],const float ambient[3],uint8_t rgb[3])
{
    float factors[3];uint8_t result[3];uint32_t i;
    if(!vector || !lights || !ambient || !rgb)return RF_RANGE;
    for(i=0;i<3;++i) {
        double dot=(double)vector[0]*lights[i][0]+(double)vector[1]*lights[i][1]+(double)vector[2]*lights[i][2];
        factors[i]=dot>=0?(float)dot:0; /* Unordered compares also select zero. */
    }
    for(i=0;i<3;++i) {
        double value=(double)lights[2][3+i]*factors[2]+(double)lights[1][3+i]*factors[1]+(double)lights[0][3+i]*factors[0]+ambient[i];
        float stored,biased;uint32_t bits;
        stored=value<=255?(float)value:255;
        biased=stored+12582912.0f;memcpy(&bits,&biased,4);result[i]=(uint8_t)bits;
    }
    memcpy(rgb,result,3);return RF_OK;
}

int rf_model_render_vertex_pair(const float position[3],const float second[3],
    const uint8_t weights[4],const uint8_t bones[4],const float (*matrices)[12],uint32_t count,float result[6])
{
    float sum[6]={0};uint32_t i,j;
    if(!position || !second || !weights || !bones || !result || (!matrices && count))return RF_RANGE;
    for(i=0;i<4 && weights[i];++i) {
        const float *m;double value[6];float stored;
        if(bones[i]>=count)return RF_RANGE;
        m=matrices[bones[i]];
        for(j=0;j<3;++j) {
            value[j]=((double)m[6+j]*position[2]+(double)m[j]*position[0])+(double)m[3+j]*position[1]+m[9+j];
            value[j+3]=((double)m[j]*second[0]+(double)m[6+j]*second[2])+(double)m[3+j]*second[1]+m[9+j];
        }
        /* Original retains position Y on x87 across accumulation; the other
         * transformed components pass through float stores. */
        for(j=0;j<6;++j) {
            if(j!=1) { stored=(float)value[j];value[j]=stored; }
            sum[j]=(float)(value[j]*weights[i]+sum[j]);
        }
    }
    for(j=0;j<6;++j)sum[j]*=1.0f/256.0f;
    memcpy(result,sum,sizeof(sum));return RF_OK;
}

int rf_model_collision_vertex(const float position[3],const uint8_t weights[4],const uint8_t bones[4],
    const float (*matrices)[12],uint32_t count,float result[3])
{
    float sum[3]={0,0,0};uint32_t i,j;
    if(!position || !weights || !bones || !result || (!matrices && count))return RF_RANGE;
    for(i=0;i<4 && weights[i];++i) {
        const float *m;float p[3],factor;
        if(bones[i]>=count)return RF_RANGE;
        m=matrices[bones[i]];factor=(float)weights[i]*(1.0f/256.0f);
        /* Preserve original 0x4ff020 addition order and float store boundaries. */
        p[0]=(float)(((double)m[3]*position[1]+(double)m[6]*position[2])+(double)m[0]*position[0]+m[9]);
        for(j=1;j<3;++j)p[j]=(float)(((double)m[3+j]*position[1]+(double)m[j]*position[0])+(double)m[6+j]*position[2]+m[9+j]);
        for(j=0;j<3;++j) { float weighted=p[j]*factor;sum[j]+=weighted; }
    }
    memcpy(result,sum,sizeof(sum));return RF_OK;
}

int rf_model_material_from_disk(rf_model_material_instance *instance,
    const uint8_t *raw,size_t size,int32_t primary_texture,int32_t secondary_texture,
    uint32_t primary_transparent,uint32_t budget)
{
    rf_model_material_record source={{0}};
    uint32_t scalar,flags,disk_flags,one=1; const uint32_t *arrays[3]={NULL,&scalar,NULL};
    const uint32_t capacities[3]={0,1,0};
    if (!instance || !raw || size!=84 || primary_transparent>1) return RF_RANGE;
    if (!raw[0] || !memchr(raw,0,32) || !memchr(raw+48,0,32)) return RF_FORMAT;
    rf_model_material_initialize(&source);
    memset(source.bytes,0,4);
    memcpy(source.bytes+0x14,raw,strlen((const char *)raw)+1);
    memcpy(source.bytes+0x10,&primary_texture,4);
    memcpy(&scalar,raw+32,4);memcpy(source.bytes+0xb8,&one,4);
    memcpy(source.bytes+0x84,raw+36,12);
    memcpy(source.bytes+0x90,raw+48,32);
    if (!raw[48]) secondary_texture=-1;
    memcpy(source.bytes+0xb4,&secondary_texture,4);
    memcpy(&disk_flags,raw+80,4);
    flags=1u | ((primary_transparent || (disk_flags&2)) ? 8u : 0u) | ((disk_flags&1) ? 16u : 0u);
    memcpy(source.bytes+4,&flags,4);source.bytes[8]=(disk_flags&2)!=0;
    return rf_model_material_instance_open(instance,&source,2,arrays,capacities,budget);
}

void rf_model_material_instance_close(rf_model_material_instance *instance)
{
    if (!instance) return;
    free(instance->storage); memset(instance,0,sizeof(*instance));
}
int rf_model_material_instance_open(rf_model_material_instance *instance,
    const rf_model_material_record *source,int32_t kind,const uint32_t *const arrays[3],
    const uint32_t capacities[3],uint32_t budget)
{
    static const unsigned offsets[]={0x7c,0xb8,0xc0};
    rf_model_material_instance next={0}; uint64_t bytes=sizeof(next); uint32_t offset=0; unsigned i; int status;
    if (!instance || !source || !arrays || !capacities || instance->storage || instance->accounted_bytes) return RF_RANGE;
    rf_model_material_initialize(&next.record);
    status=rf_model_material_prepare_copy(&next.record,source,kind,next.counts);
    if (status!=RF_OK) return status;
    for (i=0;i<3;++i) {
        if (next.counts[i]>capacities[i] || (next.counts[i] && !arrays[i])) return RF_RANGE;
        bytes+=(uint64_t)next.counts[i]*4;
    }
    if (bytes>budget || bytes>UINT32_MAX || bytes>SIZE_MAX) return RF_RANGE;
    if (bytes>sizeof(next)) {
        next.storage=malloc((size_t)(bytes-sizeof(next)));
        if (!next.storage) return RF_IO;
    }
    for (i=0;i<3;++i) if (next.counts[i]) {
        next.arrays[i]=next.storage+offset;
        memcpy(next.arrays[i],arrays[i],(size_t)next.counts[i]*4);
        memcpy(next.record.bytes+offsets[i],&next.counts[i],4);
        offset+=next.counts[i];
    }
    next.accounted_bytes=(uint32_t)bytes; *instance=next;
    return RF_OK;
}

int rf_model_material_initialize(rf_model_material_record *material)
{
    static const unsigned zeros[]={4,0x7c,0x80,0x84,0x88,0x8c,0xb8,0xbc,0xc0,0xc4};
    static const unsigned invalid[]={0,0x10,0x44,0xb4};
    unsigned i;
    if (!material) return RF_RANGE;
    for (i=0;i<sizeof(zeros)/sizeof(zeros[0]);++i) memset(material->bytes+zeros[i],0,4);
    for (i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) memset(material->bytes+invalid[i],255,4);
    memset(material->bytes+9,255,4);
    memset(material->bytes+0x78,0,4); material->bytes[0x78]=15;
    material->bytes[0x90]=0;
    return RF_OK;
}
int rf_model_material_prepare_copy(rf_model_material_record *destination,
    const rf_model_material_record *source,int32_t kind,uint32_t array_counts[3])
{
    static const unsigned names[]={0x14,0x48,0x90},counts[]={0x7c,0xb8,0xc0};
    size_t lengths[3]; unsigned i; uint32_t plan[3];
    if (!destination || !source || !array_counts || destination==source) return RF_RANGE;
    for (i=0;i<3;++i) {
        const uint8_t *end=memchr(source->bytes+names[i],0,36); int32_t count;
        if (!end) return RF_FORMAT;
        lengths[i]=(size_t)(end-(source->bytes+names[i]))+1;
        memcpy(&count,source->bytes+counts[i],4);
        plan[i]=count>0 ? (kind==3 ? (uint32_t)count : 1u) : 0u;
    }
    memcpy(destination->bytes,source->bytes,13); destination->bytes[4]|=1;
    for (i=0;i<2;++i) {
        unsigned offset=0x10+i*0x34;
        memcpy(destination->bytes+offset,source->bytes+offset,4);
        memcpy(destination->bytes+offset+0x28,source->bytes+offset+0x28,12);
    }
    for (i=0;i<3;++i) memcpy(destination->bytes+names[i],source->bytes+names[i],lengths[i]);
    memcpy(destination->bytes+0x78,source->bytes+0x78,4);
    memcpy(destination->bytes+0x84,source->bytes+0x84,12);
    memcpy(destination->bytes+0xb4,source->bytes+0xb4,4);
    memcpy(array_counts,plan,sizeof(plan));
    return RF_OK;
}

int rf_model_material_count(int32_t kind,int32_t static_lods,int32_t static_count,
    int32_t mesh_count,const int32_t *mesh_counts,uint32_t mesh_capacity,
    int32_t direct_count,int32_t *result)
{
    int32_t value=0; uint32_t sum=0,i;
    if (!result) return RF_RANGE;
    if (kind==1) { if (static_lods<=1) value=static_count; }
    else if (kind==3) value=direct_count;
    else if (kind==2 && mesh_count>0) {
        if (!mesh_counts || (uint32_t)mesh_count>mesh_capacity) return RF_RANGE;
        for (i=0;i<(uint32_t)mesh_count;++i) sum+=(uint32_t)mesh_counts[i];
        memcpy(&value,&sum,4);
    }
    *result=value; return RF_OK;
}

static int valid_name(rf_model_name name)
{
    size_t i;
    if (!name.data && name.length) return 0;
    for (i = 0; i < name.length; ++i)
        if (!name.data[i]) return 0;
    return 1;
}

static unsigned char fold(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

int rf_model_find_tag(const rf_model_name_group groups[3],
                      rf_model_name query, int32_t *index)
{
    uint32_t g, n, base = 0, total = 0;
    if (!groups || !index) return RF_RANGE;
    if (!valid_name(query)) return RF_FORMAT;
    for (g = 0; g < 3; ++g) {
        if ((groups[g].count && !groups[g].names) ||
            groups[g].count > (uint32_t)INT32_MAX - total) return RF_RANGE;
        total += groups[g].count;
    }
    /* Reconstructed from RF.exe 0x51d5b0 and default-locale 0x57c130.
     * Stop at the first match, including duplicates across groups. */
    for (g = 0; g < 3; ++g) {
        for (n = 0; n < groups[g].count; ++n) {
            rf_model_name name = groups[g].names[n];
            size_t i;
            if (!valid_name(name)) return RF_FORMAT;
            if (name.length != query.length) continue;
            for (i = 0; i < name.length; ++i)
                if (fold((unsigned char)name.data[i]) != fold((unsigned char)query.data[i])) break;
            if (i == name.length) {
                *index = (int32_t)(base + n);
                return RF_OK;
            }
        }
        base += groups[g].count;
    }
    return RF_NOT_FOUND;
}

static uint32_t read_word(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int rf_model_decode_bones(const void *payload, size_t bytes,
                          rf_model_bone *bones, uint32_t capacity, uint32_t *count)
{
    const unsigned char *p = payload;
    uint32_t total, i, j;
    if (!p || !count) return RF_RANGE;
    if (bytes < 4) return RF_FORMAT;
    total = read_word(p);
    if (total > INT32_MAX || (bytes - 4) / 56 != total || (bytes - 4) % 56) return RF_FORMAT;
    if (total > capacity || (total && !bones)) return RF_RANGE;
    /* Validate the entire payload before modifying caller output. Parent walks
     * are bounded by total, catching cycles without temporary allocation. */
    for (i = 0; i < total; ++i) {
        const unsigned char *record = p + 4 + (size_t)i * 56;
        uint32_t parent = i, steps = 0;
        for (j = 0; j < 7; ++j) {
            uint32_t bits = read_word(record + 24 + j * 4);
            float value;
            memcpy(&value, &bits, 4);
            if (!isfinite(value)) return RF_FORMAT;
        }
        while (parent != UINT32_MAX) {
            if (parent >= total || steps++ >= total) return RF_FORMAT;
            parent = read_word(p + 4 + (size_t)parent * 56 + 52);
        }
    }
    for (i = 0; i < total; ++i) {
        const unsigned char *record = p + 4 + (size_t)i * 56;
        for (j = 0; j < 24; ++j) bones[i].name[j] = (char)record[j];
        bones[i].name[24] = 0;
        for (j = 0; j < 7; ++j) {
            uint32_t bits = read_word(record + 24 + j * 4);
            float *out = j < 4 ? &bones[i].rotation[j] : &bones[i].position[j - 4];
            memcpy(out, &bits, 4);
        }
        {
            uint32_t bits = read_word(record + 52);
            memcpy(&bones[i].parent, &bits, 4);
        }
    }
    *count = total;
    return RF_OK;
}

static int make_transform(const float rotation[4], const float position[3], float transform[12], int normalize)
{
    double length = 0, scale, x, y, z, w;
    float q[4], result[12], wy, zx, one_minus_xx;
    uint32_t i;
    if (!rotation || !position || !transform) return RF_RANGE;
    for (i = 0; i < 4; ++i) {
        if (!isfinite(rotation[i])) return RF_FORMAT;
        length += (double)rotation[i] * rotation[i];
    }
    for (i = 0; i < 3; ++i) if (!isfinite(position[i])) return RF_FORMAT;
    if (normalize && length == 0) return RF_FORMAT;
    scale = normalize ? sqrt(1.0 / length) : 1.0;
    for (i = 0; i < 4; ++i) q[i] = (float)((double)rotation[i] * scale);
    x = q[0]; y = q[1]; z = q[2]; w = q[3];
    /* Match binary32 spills visible in original 0x5193f0, including the
     * asymmetric rounding of XZ/WY terms and the shared diagonal term. */
    wy = (float)(w * y); zx = (float)(z * x);
    one_minus_xx = (float)(1.0 - 2.0 * x * x);
    result[0] = (float)((1.0 - 2.0 * y * y) - 2.0 * z * z);
    result[1] = (float)(2.0 * x * y - 2.0 * w * z);
    result[2] = (float)(2.0 * (z * x + wy));
    result[3] = (float)(2.0 * (w * z + x * y));
    result[4] = (float)((double)one_minus_xx - 2.0 * z * z);
    result[5] = (float)(2.0 * z * y - 2.0 * w * x);
    result[6] = (float)(2.0 * zx - 2.0 * wy);
    result[7] = (float)(2.0 * (w * x + z * y));
    result[8] = (float)((double)one_minus_xx - 2.0 * y * y);
    for (i = 0; i < 3; ++i) result[i + 9] = position[i];
    for (i = 0; i < 12; ++i) if (!isfinite(result[i])) return RF_RANGE;
    memcpy(transform, result, sizeof(result));
    return RF_OK;
}

int rf_model_bone_transform(const float rotation[4], const float position[3], float transform[12])
{
    return make_transform(rotation, position, transform, 1);
}

int rf_model_attachment_transform(const float rotation[4], const float position[3], float transform[12])
{
    return make_transform(rotation, position, transform, 0);
}

int rf_model_compose_transform(const float local[12], const float parent[12], float result[12])
{
    float out[12];
    uint32_t i;
    if (!local || !parent || !result) return RF_RANGE;
    for (i = 0; i < 12; ++i)
        if (!isfinite(local[i]) || !isfinite(parent[i])) return RF_FORMAT;
    for (i = 0; i < 4; ++i) {
        double x = local[i * 3], y = local[i * 3 + 1], z = local[i * 3 + 2];
        double w = i == 3 ? 1.0 : 0.0;
        /* Preserve each column's distinct accumulation order in 0x51c620. */
        out[i * 3] = (float)(((z * parent[6] + w * parent[9]) + x * parent[0]) + y * parent[3]);
        out[i * 3 + 1] = (float)(((w * parent[10] + z * parent[7]) + x * parent[1]) + y * parent[4]);
        out[i * 3 + 2] = (float)(((z * parent[8] + x * parent[2]) + w * parent[11]) + y * parent[5]);
    }
    for (i = 0; i < 12; ++i) if (!isfinite(out[i])) return RF_RANGE;
    memcpy(result, out, sizeof(out));
    return RF_OK;
}

int rf_model_bone_order(const rf_model_bone *bones, uint32_t count, uint8_t *order, uint32_t capacity)
{
    uint16_t depths[256];
    uint8_t sorted[256];
    uint32_t i, depth, written = 0;
    if (count > 256 || count > capacity || (count && (!bones || !order))) return RF_RANGE;
    for (i = 0; i < count; ++i) {
        int32_t parent = bones[i].parent;
        depth = 0;
        while (parent != -1) {
            if (parent < 0 || (uint32_t)parent >= count || ++depth >= count) return RF_FORMAT;
            parent = bones[parent].parent;
        }
        depths[i] = (uint16_t)depth;
    }
    for (depth = 0; written < count; ++depth)
        for (i = 0; i < count; ++i)
            if (depths[i] == depth) sorted[written++] = (uint8_t)i;
    if (count) memcpy(order, sorted, count);
    return RF_OK;
}
/* Float quaternion path 0x519da0, distinct from packed key interpolation. */
static double pose_dot(const float a[4], const float b[4])
{
    return (((double)a[3]*b[3] + (double)a[2]*b[2]) + (double)a[1]*b[1]) + (double)a[0]*b[0];
}
static int pose_interpolate(const float a[4], const float b[4], float t, float out[4])
{
    float difference[4], sum[4], second[4], dot;
    double wa, wb, value; unsigned i; int opposite;
    for (i=0;i<4;++i) { difference[i]=a[i]-b[i]; sum[i]=a[i]+b[i]; second[i]=b[i]; }
    if (pose_dot(sum,sum)<=(float)pose_dot(difference,difference))
        for (i=0;i<4;++i) second[i]=-second[i];
    dot=(float)pose_dot(a,second);
    if (!isfinite(dot)) return RF_RANGE;
    opposite=(double)dot+1<=(double)1.0e-6f;
    if (opposite) {
        wa=sin((1.0-t)*(double)1.5707963705062866f); wb=sin((double)t*(double)1.5707963705062866f);
    } else if (1.0-dot<=(double)1.0e-6f) { wa=0; wb=1; }
    else {
        double angle=acos(dot); float rounded=(float)angle, reciprocal=(float)(1.0/sin(angle));
        wa=sin((1.0-t)*rounded)*reciprocal; wb=sin((double)t*rounded)*reciprocal;
    }
    for (i=0;i<4;++i) {
        value=(double)a[i]*wa;
        value+=(opposite ? ((i&1) ? second[i-1] : -second[i+1]) : second[i])*wb;
        out[i]=(float)value;
        if (!isfinite(out[i])) return RF_RANGE;
        if (i==3 && value==0) out[i]=1.0e-6f;
    }
    return RF_OK;
}
int rf_model_blend_pose(const float (*rotations)[4], const float (*positions)[3], const float *weights,
                        uint32_t count, float out[12])
{
    float q[4]={0,0,0,1}, p[3]={0,0,0}, matrix[12], cumulative=0; uint32_t i,c; int status;
    if (!rotations || !positions || !weights || !out || !count || count>16) return RF_RANGE;
    for (i=0;i<count;++i) {
        if (!isfinite(weights[i]) || weights[i]<=0 || weights[i]>1) return RF_FORMAT;
        for (c=0;c<4;++c) if (!isfinite(rotations[i][c])) return RF_FORMAT;
        for (c=0;c<3;++c) if (!isfinite(positions[i][c])) return RF_FORMAT;
    }
    if (count==1) { memcpy(q,rotations[0],sizeof(q)); memcpy(p,positions[0],sizeof(p)); }
    else {
        if (count==2) {
            for (c=0;c<3;++c) {
                float first=positions[0][c]*weights[0], second=positions[1][c]*weights[1];
                p[c]=first+second;
            }
            status=pose_interpolate(rotations[0],rotations[1],weights[1],q);
        }
        else {
            for (i=0;i<count;++i) for (c=0;c<3;++c) {
                float product=positions[i][c]*weights[i]; p[c]+=product;
            }
            status=RF_OK;
            for (i=0;i<count && status==RF_OK;++i) {
                float next[4]; cumulative+=weights[i];
                status=pose_interpolate(q,rotations[i],weights[i]/cumulative,next);
                if (status==RF_OK) memcpy(q,next,sizeof(q));
            }
        }
        if (status!=RF_OK) return status;
    }
    status=rf_model_attachment_transform(q,p,matrix); if (status!=RF_OK) return status;
    if (count>2 && matrix[0]==0) matrix[0]=1.0e-6f;
    memcpy(out,matrix,sizeof(matrix)); return RF_OK;
}
int rf_model_sample_single_motion(const rf_model_bone *bones, uint32_t count, const rf_motion_file *motion,
                                  int32_t tick, int bypass_fades, float (*matrices)[12], uint32_t capacity)
{
    uint8_t order[256]; uint32_t i,index; int status;
    if (!bones || !motion || !matrices || !count || count>256 || capacity<count) return RF_RANGE;
    if (motion->header[6]!=count) return RF_FORMAT;
    status=rf_model_bone_order(bones,count,order,sizeof(order)); if (status!=RF_OK) return status;
    for (i=0;i<count;++i) {
        rf_motion_sample sample; float local[12];
        const float identity[4]={0,0,0,1}, zero[3]={0,0,0};
        index=order[i];
        status=rf_motion_file_sample(motion,index,tick,bypass_fades,&sample); if (status!=RF_OK) return status;
        if (sample.weight>0) status=rf_model_attachment_transform(sample.rotation,sample.position,local);
        else status=rf_model_attachment_transform(identity,zero,local);
        if (status!=RF_OK) return status;
        if (bones[index].parent<0) {
            /* 0x51b500 adds the instance's root displacement even when zero. */
            local[9]=0.0f+local[9]; local[10]=0.0f+local[10]; local[11]=0.0f+local[11];
            memcpy(matrices[index],local,sizeof(local));
        }
        else {
            status=rf_model_compose_transform(local,matrices[bones[index].parent],matrices[index]);
            if (status!=RF_OK) return status;
        }
    }
    return RF_OK;
}
static int model_sample_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                             const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                             uint32_t resource_count, float root_displacement[3], float (*matrices)[12], uint16_t *generations, uint32_t capacity)
{
    uint8_t order[256]; uint32_t i,j,index,mask=0; int status;
    const rf_motion_slot_state *active;
    if (!bones || !state || !root_displacement || !matrices || !count || count>256 || capacity<count) return RF_RANGE;
    if (generations && state->generation>65535) return RF_FORMAT;
    for (j=0;j<3;++j) if (!isfinite(root_displacement[j])) return RF_FORMAT;
    active=&state->completion.active;
    if (active->count>16 || (active->count && (!motions || !resources))) return RF_RANGE;
    for (j=0;j<active->count;++j) {
        int32_t id=active->slots[j].motion;
        if (id<0 || (uint32_t)id>=resource_count || !motions[id] || motions[id]->header[6]!=count) return RF_FORMAT;
        if (resources[id].looping) mask|=1u<<j;
    }
    status=rf_model_bone_order(bones,count,order,sizeof(order)); if (status!=RF_OK) return status;
    for (i=0;i<count;++i) {
        rf_motion_weight_envelope envelopes[16]; float weights[16], compact[16], rotations[16][4], positions[16][3], local[12];
        uint32_t contributions=0;
        const float identity[4]={0,0,0,1}, zero[3]={0,0,0};
        index=order[i];
        if (generations && generations[index]==(uint16_t)state->generation) continue;
        for (j=0;j<active->count;++j) {
            rf_motion_track track;
            status=rf_motion_file_track(motions[active->slots[j].motion],index,&track); if (status!=RF_OK) return status;
            envelopes[j]=track.envelope;
        }
        status=rf_motion_bone_weights(active,envelopes,mask,weights); if (status!=RF_OK) return status;
        for (j=0;j<active->count;++j) if (weights[j]>0) {
            rf_motion_sample sample;
            status=rf_motion_file_sample(motions[active->slots[j].motion],index,active->slots[j].tick,(mask & (1u<<j))!=0,&sample);
            if (status!=RF_OK) return status;
            memcpy(rotations[contributions],sample.rotation,sizeof(sample.rotation));
            memcpy(positions[contributions],sample.position,sizeof(sample.position));
            compact[contributions++]=weights[j];
        }
        if (contributions) status=rf_model_blend_pose((const float (*)[4])rotations,(const float (*)[3])positions,compact,contributions,local);
        else status=rf_model_attachment_transform(identity,zero,local);
        if (status!=RF_OK) return status;
        if (bones[index].parent<0) {
            /* 0x51b8c7..0x51b924: add pending displacement, then consume it.
             * Later roots in this evaluation receive positive zero. */
            for (j=0;j<3;++j) {
                local[9+j]=root_displacement[j]+local[9+j];
                if (!isfinite(local[9+j])) return RF_RANGE;
            }
            root_displacement[0]=root_displacement[1]=root_displacement[2]=0;
            memcpy(matrices[index],local,sizeof(local));
        } else {
            status=rf_model_compose_transform(local,matrices[bones[index].parent],matrices[index]);
            if (status!=RF_OK) return status;
        }
        if (generations) generations[index]=(uint16_t)state->generation;
    }
    return RF_OK;
}

int rf_model_sample_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                             const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                             uint32_t resource_count, float root_displacement[3], float (*matrices)[12], uint32_t capacity)
{
    return model_sample_playback(bones,count,state,motions,resources,resource_count,root_displacement,matrices,NULL,capacity);
}

int rf_model_evaluate_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                               const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                               uint32_t resource_count, float root_displacement[3], float (*matrices)[12],
                               uint16_t *generations, uint32_t capacity)
{
    if (!generations) return RF_RANGE;
    return model_sample_playback(bones,count,state,motions,resources,resource_count,root_displacement,matrices,generations,capacity);
}

int rf_model_place_tag(const float local[12], const float orientation[9], const float position[3], float out[12])
{
    float result[12]; double a,b,c; unsigned i;
    if (!local || !orientation || !position || !out) return RF_RANGE;
    for (i=0;i<12;++i) if (!isfinite(local[i])) return RF_FORMAT;
    for (i=0;i<9;++i) if (!isfinite(orientation[i])) return RF_FORMAT;
    for (i=0;i<3;++i) if (!isfinite(position[i])) return RF_FORMAT;
    for (i=0;i<9;++i) {
        unsigned row=i/3, col=i%3;
        a=(double)local[row*3]*orientation[col];
        b=(double)local[row*3+1]*orientation[col+3];
        c=(double)local[row*3+2]*orientation[col+6];
        if (i==0) result[i]=(float)((b+c)+a);
        else if (i==2 || i==8) result[i]=(float)((c+a)+b);
        else if (i==5) result[i]=(float)((c+b)+a);
        else if (i==7) result[i]=(float)((b+a)+c);
        else result[i]=(float)((a+b)+c);
    }
    for (i=0;i<3;++i) {
        float rotated=(float)(((double)local[9]*orientation[i]+(double)local[10]*orientation[i+3])+
                              (double)local[11]*orientation[i+6]);
        result[9+i]=rotated+position[i];
    }
    for (i=0;i<12;++i) if (!isfinite(result[i])) return RF_RANGE;
    memcpy(out,result,sizeof(result)); return RF_OK;
}
