#include "rf/cutscene.h"
#include "rf/timer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct cut_blob {unsigned char *data;uint32_t size,count;} cut_blob;
typedef struct cut_reader {const unsigned char *data;uint32_t size,at;} cut_reader;
typedef struct cut_control {uint32_t uid;float position[3];} cut_control;
static uint32_t cut_u32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int cut_take(cut_reader *r,uint32_t size,const unsigned char **value)
{if(size>r->size-r->at)return RF_FORMAT;*value=r->data+r->at;r->at+=size;return RF_OK;}
static int cut_word(cut_reader *r,uint32_t *value)
{const unsigned char *p;int status=cut_take(r,4,&p);if(!status)*value=cut_u32(p);return status;}
static int cut_float(cut_reader *r,float *value)
{uint32_t word;int status=cut_word(r,&word);if(status)return status;memcpy(value,&word,4);return isfinite(*value)?RF_OK:RF_FORMAT;}
static int cut_string(cut_reader *r,char *value)
{
    const unsigned char *p;uint32_t size;int status=cut_take(r,2,&p);
    if(status)return status;size=(uint32_t)p[0]|((uint32_t)p[1]<<8);
    if(size>=64)return RF_RANGE;status=cut_take(r,size,&p);if(status)return status;
    if(memchr(p,0,size))return RF_FORMAT;
    if(value){memcpy(value,p,size);value[size]=0;}return RF_OK;
}
static int cut_blob_read(const rf_level *level,uint32_t type,uint32_t limit,cut_blob *blob)
{
    const rf_level_section *section=rf_level_find(level,type);uint32_t count;int status;
    if(!section)return RF_NOT_FOUND;
    if(section->size<4 || section->size>65536)return RF_RANGE;
    blob->data=malloc(section->size);if(!blob->data)return RF_IO;
    blob->size=section->size;status=rf_level_read(level,section,0,blob->data,blob->size);
    if(status)return status;
    count=cut_u32(blob->data);if(count>limit)return RF_RANGE;
    blob->count=count;return RF_OK;
}
static void cut_blobs_close(cut_blob blobs[4])
{uint32_t i;for(i=0;i<4;i++)free(blobs[i].data);}
static int cut_camera_records(const cut_blob *blob,rf_cutscene_camera *output,cut_control *controls)
{
    cut_reader r={blob->data,blob->size,4};uint32_t i,j,uid;float position[3],disk[9];int status;
    if(!blob->data)return RF_OK;
    for(i=0;i<blob->count;i++) {
        status=cut_word(&r,&uid);if(status)return status;
        status=cut_string(&r,NULL);if(status)return status;
        for(j=0;j<3;j++){status=cut_float(&r,position+j);if(status)return status;}
        for(j=0;j<9;j++){status=cut_float(&r,disk+j);if(status)return status;}
        status=cut_string(&r,NULL);if(status)return status;
        {const unsigned char *flag;status=cut_take(&r,1,&flag);if(status)return status;(void)flag;}
        if(output){output[i].uid=uid;memcpy(output[i].position,position,sizeof(position));
            for(j=0;j<9;j++)output[i].orientation[j]=disk[(j+3)%9];}
        if(controls){controls[i].uid=uid;memcpy(controls[i].position,position,sizeof(position));}
    }
    return r.at==r.size?RF_OK:RF_FORMAT;
}
static int cut_timelines(const cut_blob *blob,rf_cutscene_resources *resources,uint32_t *points)
{
    cut_reader r={blob->data,blob->size,4};uint32_t i,j,k,n,total=0;int status;
    for(i=0;i<blob->count;i++) {
        uint32_t uid;float fov;const unsigned char *hide;
        status=cut_word(&r,&uid);if(status)return status;
        status=cut_take(&r,1,&hide);if(status)return status;
        status=cut_float(&r,&fov);if(status)return status;
        if(fov<=0 || fov>180)return RF_FORMAT;
        status=cut_word(&r,&n);if(status)return status;
        if(n>64 || n>1024-total)return RF_RANGE;
        if(resources){rf_cutscene_descriptor *d=resources->descriptors+i;
            d->selector=uid;d->hide=*hide;d->fov=fov;d->first_point=total;d->point_count=n;}
        for(j=0;j<n;j++) {
            rf_cutscene_point *p=resources?resources->points+total+j:NULL;uint32_t camera;
            status=cut_word(&r,&camera);if(status)return status;
            if(p)p->camera_uid=camera;
            for(k=0;k<3;k++){float duration;status=cut_float(&r,&duration);if(status)return status;
                if(duration<0)return RF_FORMAT;if(p)p->durations[k]=duration;}
            for(k=0;k<2;k++){uint32_t word;status=cut_word(&r,&word);if(status)return status;if(p)p->words[k]=word;}
            status=cut_string(&r,p?p->path:NULL);if(status)return status;
        }
        total+=n;
    }
    if(r.at!=r.size)return RF_FORMAT;*points=total;return RF_OK;
}
static int cut_path_records(const cut_blob *blob,rf_cutscene_path *paths,
    const cut_control *controls,uint32_t control_count)
{
    cut_reader r={blob->data,blob->size,4};uint32_t i,j,k,count,uid;int status;
    if(!blob->data)return RF_OK;
    for(i=0;i<blob->count;i++) {
        status=cut_string(&r,paths[i].name);if(status)return status;
        status=cut_word(&r,&count);if(status)return status;
        if(count!=4)return RF_FORMAT;
        for(j=0;j<4;j++) {
            status=cut_word(&r,&uid);if(status)return status;
            for(k=0;k<control_count && controls[k].uid!=uid;k++);
            if(k==control_count)return RF_FORMAT;
            paths[i].control_uids[j]=uid;
            memcpy(paths[i].positions[j],controls[k].position,sizeof(controls[k].position));
        }
    }
    return r.at==r.size?RF_OK:RF_FORMAT;
}
static int cut_equal(const char *a,const char *b)
{
    while(*a && *b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 0;}
    return !*a && !*b;
}
const rf_cutscene_descriptor *rf_cutscene_find(const rf_cutscene_resources *r,uint32_t selector)
{uint32_t i;if(!r)return NULL;for(i=0;i<r->descriptor_count;i++)if(r->descriptors[i].selector==selector)return r->descriptors+i;return NULL;}
const rf_cutscene_camera *rf_cutscene_camera_find(const rf_cutscene_resources *r,uint32_t uid)
{uint32_t i;if(!r)return NULL;for(i=0;i<r->camera_count;i++)if(r->cameras[i].uid==uid)return r->cameras+i;return NULL;}
const rf_cutscene_path *rf_cutscene_path_find(const rf_cutscene_resources *r,const char *name)
{uint32_t i;if(!r || !name)return NULL;for(i=0;i<r->path_count;i++)if(cut_equal(r->paths[i].name,name))return r->paths+i;return NULL;}
void rf_cutscene_path_sample(const rf_cutscene_path *path,float t,float position[3])
{
    float q,w[4];uint32_t j;if(!path || !position)return;q=1-t;
    w[0]=q*q*q;w[1]=3*t*q*q;w[2]=3*t*t*q;w[3]=t*t*t;
    for(j=0;j<3;j++)position[j]=w[0]*path->positions[0][j]+w[1]*path->positions[1][j]+w[2]*path->positions[2][j]+w[3]*path->positions[3][j];
}
void rf_cutscene_resources_close(rf_cutscene_resources *r)
{if(!r)return;free(r->storage);memset(r,0,sizeof(*r));}
int rf_cutscene_resources_open(const rf_level *level,uint32_t budget,rf_cutscene_resources *result)
{
    static const uint32_t types[4]={0x400,0x4000,0x5000,0x6000};
    static const uint32_t limits[4]={128,16,64,32};
    cut_blob blobs[4]={{0}};cut_control controls[64]={{0}};rf_cutscene_resources next={0};
    uint32_t i,points=0;uint64_t size=0,peak=0;unsigned char *base;int status;
    if(!level || !result || result->storage)return RF_RANGE;
    for(i=0;i<4;i++) {
        status=cut_blob_read(level,types[i],limits[i],blobs+i);
        if(status==RF_NOT_FOUND){if(i==1)goto done;continue;}
        if(status)goto done;peak+=blobs[i].size;
    }
    status=cut_timelines(blobs+1,NULL,&points);if(status)goto done;
    size=(uint64_t)blobs[0].count*sizeof(rf_cutscene_camera)+
        (uint64_t)blobs[1].count*sizeof(rf_cutscene_descriptor)+
        (uint64_t)points*sizeof(rf_cutscene_point)+
        (uint64_t)blobs[3].count*sizeof(rf_cutscene_path);
    if(size+peak>budget || size>UINT32_MAX){status=RF_RANGE;goto done;}
    next.storage=calloc(1,(size_t)(size?size:1));if(!next.storage){status=RF_IO;goto done;}
    base=next.storage;next.cameras=(rf_cutscene_camera*)base;base+=blobs[0].count*sizeof(*next.cameras);
    next.descriptors=(rf_cutscene_descriptor*)base;base+=blobs[1].count*sizeof(*next.descriptors);
    next.points=(rf_cutscene_point*)base;base+=points*sizeof(*next.points);
    next.paths=(rf_cutscene_path*)base;
    next.camera_count=blobs[0].count;next.descriptor_count=blobs[1].count;
    next.point_count=points;next.path_count=blobs[3].count;next.allocated_bytes=(uint32_t)size;
    status=cut_camera_records(blobs,next.cameras,NULL);if(status)goto done;
    status=cut_camera_records(blobs+2,NULL,controls);if(status)goto done;
    status=cut_path_records(blobs+3,next.paths,controls,blobs[2].count);if(status)goto done;
    status=cut_timelines(blobs+1,&next,&points);if(status)goto done;
    for(i=0;i<next.point_count;i++) {
        const rf_cutscene_point *point=next.points+i;
        if(!rf_cutscene_camera_find(&next,point->camera_uid) ||
           (point->path[0] && !cut_equal(point->path,"none") && !rf_cutscene_path_find(&next,point->path))) {
            status=RF_FORMAT;goto done;
        }
    }
    *result=next;memset(&next,0,sizeof(next));status=RF_OK;
done:
    rf_cutscene_resources_close(&next);cut_blobs_close(blobs);return status;
}
static int cut_duration(float seconds,int32_t *milliseconds)
{
    double value=(double)seconds*982.7238159179688+0.5;
    if(!isfinite(seconds) || seconds<0 || value>RF_TIMER_PERIOD)return RF_RANGE;
    *milliseconds=(int32_t)value;return RF_OK;
}
static int cut_point_begin(rf_cutscene_runtime *runtime,int32_t now,uint32_t *action_uid)
{
    const rf_cutscene_descriptor *d=runtime->resources->descriptors+runtime->descriptor_index;
    const rf_cutscene_point *point=runtime->resources->points+d->first_point+runtime->point_index;
    const rf_cutscene_camera *camera=rf_cutscene_camera_find(runtime->resources,point->camera_uid);
    int32_t total,pre;int status;
    if(!camera)return RF_FORMAT;
    status=cut_duration(point->durations[0]+point->durations[1]+point->durations[2],&total);if(status)return status;
    status=cut_duration(point->durations[0],&pre);if(status)return status;
    status=rf_timer_set(&runtime->total_deadline,now,total);if(status)return status;
    status=rf_timer_set(&runtime->pre_deadline,now,pre);if(status)return status;
    runtime->move_deadline=-1;runtime->moving=0;runtime->elapsed=0;
    memcpy(runtime->position,camera->position,sizeof(runtime->position));
    memcpy(runtime->orientation,camera->orientation,sizeof(runtime->orientation));
    *action_uid=point->words[1];return RF_OK;
}
int rf_cutscene_begin(rf_cutscene_runtime *runtime,const rf_cutscene_resources *resources,
    uint32_t selector,int32_t now,uint32_t *action_uid)
{
    rf_cutscene_runtime next={0};const rf_cutscene_descriptor *d;
    uint32_t i;int status;
    if(!runtime || !resources || !action_uid || runtime->active)return RF_RANGE;
    d=rf_cutscene_find(resources,selector);if(!d)return RF_NOT_FOUND;
    if(!d->point_count)return RF_FORMAT;
    for(i=0;i<resources->descriptor_count && resources->descriptors+i!=d;i++);
    next.resources=resources;next.active_uid=selector;next.descriptor_index=i;
    next.active=1;next.fov=d->fov;
    status=cut_point_begin(&next,now,action_uid);if(status)return status;
    *runtime=next;return RF_OK;
}
int rf_cutscene_step(rf_cutscene_runtime *runtime,int32_t now,float seconds,
    uint32_t *action_uid,uint32_t *finished)
{
    const rf_cutscene_descriptor *d;const rf_cutscene_point *point;
    const rf_cutscene_path *path;int expired,status;
    if(!runtime || !action_uid || !finished || !isfinite(seconds) || seconds<0)return RF_RANGE;
    *action_uid=UINT32_MAX;*finished=0;if(!runtime->active)return RF_OK;
    d=runtime->resources->descriptors+runtime->descriptor_index;
    point=runtime->resources->points+d->first_point+runtime->point_index;
    status=rf_timer_expired(runtime->total_deadline,now,&expired);if(status)return status;
    if(expired) {
        if(++runtime->point_index==d->point_count){rf_cutscene_cancel(runtime);*finished=1;return RF_OK;}
        return cut_point_begin(runtime,now,action_uid);
    }
    path=point->path[0]?rf_cutscene_path_find(runtime->resources,point->path):NULL;
    if(!path)return RF_OK;
    if(!runtime->moving) {
        if(runtime->pre_deadline==-1)return RF_OK; /* Path already finished in this point. */
        status=rf_timer_expired(runtime->pre_deadline,now,&expired);if(status)return status;
        if(!expired)return RF_OK;
        runtime->pre_deadline=-1;runtime->elapsed=0;runtime->moving=1;
        {int32_t offset;status=cut_duration(point->durations[1],&offset);if(status)return status;
         status=rf_timer_set(&runtime->move_deadline,now,offset);if(status)return status;}
    }
    runtime->elapsed+=seconds*0.9827237725257874f;
    if(point->durations[1]>0)rf_cutscene_path_sample(path,runtime->elapsed/point->durations[1],runtime->position);
    status=rf_timer_expired(runtime->move_deadline,now,&expired);if(status)return status;
    if(expired){runtime->moving=0;runtime->move_deadline=-1;}
    return RF_OK;
}
void rf_cutscene_cancel(rf_cutscene_runtime *runtime)
{if(runtime)memset(runtime,0,sizeof(*runtime));}
