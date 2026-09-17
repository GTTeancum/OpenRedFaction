/* Include after checkpoint_u32/checkpoint_float. This is a bounded RFDS2
 * admission reader, not a source-identity validator or live scene publisher.
 * Output is caller-owned (heap/stage, about6KiB), never a large stack temporary.
 * Validate every record first, then copy without fallible work. Input/layout/
 * expected bounds/output must not alias. No allocation or global mutation.
 */
#include "rf/authored_checkpoint_layout.h"
#include "rf/geomod.h"
#include "rf/random.h"
typedef struct scene_authored_admission_state {
    rf_geomod_shallow_history history[128];
    uint16_t requested[128][3];
    uint32_t count;rf_random_state random;
} scene_authored_admission_state;
static int scene_authored_admission_import_mode(const unsigned char *data,
    const rf_authored_checkpoint_layout *layout,const float minimum[3],
    const float maximum[3],scene_authored_admission_state *out,uint32_t collection)
{
    rf_authored_checkpoint_layout checked;uint32_t i,j,k;uintptr_t input,output;int status;
    if(!data || !layout || !minimum || !maximum || !out)return RF_RANGE;
    input=(uintptr_t)data;output=(uintptr_t)out;
    if(input>UINTPTR_MAX-layout->bytes || output>UINTPTR_MAX-sizeof(*out) ||
       (input<output+sizeof(*out) && output<input+layout->bytes))return RF_RANGE;
    status=collection?rf_authored_collection_layout_read(data,layout->bytes,&checked):rf_authored_checkpoint_layout_read(data,layout->bytes,&checked);if(status)return status;
    if(memcmp(&checked,layout,sizeof(checked)))return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(minimum[i]) || !isfinite(maximum[i]) || maximum[i]<=minimum[i] ||
           checkpoint_float(data+216+i*4)!=minimum[i] || checkpoint_float(data+228+i*4)!=maximum[i])return RF_FORMAT;
    }
    for(i=0;i<layout->admissions;i++) {
        const unsigned char *p=data+layout->admission_offset+i*48;
        for(j=0;j<10;j++)if(!isfinite(checkpoint_float(p+j*4)))return RF_FORMAT;
        if(checkpoint_float(p+36)<=0 || p[46] || p[47])return RF_FORMAT;
    }
    /* Preserve the signed depth vectors and adjusted center exactly. A packed
     * requested center is a DIFFERENT value: never regenerate it from center,
     * clamp it, or deduplicate records here. Failed CSG admissions remain. Any
     * uint32 cut-basis RNG (including0) is valid and must not be reseeded. */
    memset(out,0,sizeof(*out));
    for(i=0;i<layout->admissions;i++) {
        const unsigned char *p=data+layout->admission_offset+i*48;
        rf_geomod_shallow_history *h=out->history+i;
        for(j=0;j<3;j++)h->center[j]=checkpoint_float(p+j*4);
        for(j=0;j<2;j++)for(k=0;k<3;k++)h->vectors[j][k]=checkpoint_float(p+12+(j*3+k)*4);
        h->scale=checkpoint_float(p+36);
        for(j=0;j<3;j++)out->requested[i][j]=(uint16_t)(p[40+j*2]|((uint16_t)p[41+j*2]<<8));
    }
    out->count=layout->admissions;out->random.value=checkpoint_u32(data+244);return RF_OK;
}

static int scene_authored_admission_import(const unsigned char *data,const rf_authored_checkpoint_layout *layout,
    const float minimum[3],const float maximum[3],scene_authored_admission_state *out)
{return scene_authored_admission_import_mode(data,layout,minimum,maximum,out,0);}
static inline int scene_authored_collection_admission_import(const unsigned char *data,const rf_authored_checkpoint_layout *layout,
    const float minimum[3],const float maximum[3],scene_authored_admission_state *out)
{return scene_authored_admission_import_mode(data,layout,minimum,maximum,out,1);}
