#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/clutter.h"
#include "rf/glare.h"
#include "rf/timer.h"
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
/*48766a..4876d3: local attachments use the inverse pending orientation,
 * not its transpose. Preserve the original cofactor stores and dot order. */
int rf_attachment_local_pose(const float local[12],const float position[3],
    const float current[9],const float pending[9],float out[12])
{
    static const unsigned char order[9][3]={{0,2,1},{2,1,0},{1,0,2},
        {2,1,0},{2,1,0},{0,1,2},{2,1,0},{2,0,1},{1,0,2}};
    float inverse[9],value[12],det,minor;double determinant,cofactor[9],terms[3];unsigned i,j,k;
    if(!local || !position || !current || !pending || !out)return RF_RANGE;
    for(i=0;i<12;++i)if(!isfinite(local[i]))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(position[i]))return RF_FORMAT;
    for(i=0;i<9;++i)if(!isfinite(current[i]) || !isfinite(pending[i]))return RF_FORMAT;
    determinant=((double)pending[8]*pending[0])*pending[4];
    determinant+=((double)pending[3]*pending[2])*pending[7];
    determinant+=((double)pending[5]*pending[1])*pending[6];
    determinant-=((double)pending[2]*pending[6])*pending[4];
    determinant-=((double)pending[5]*pending[0])*pending[7];
    determinant-=((double)pending[3]*pending[1])*pending[8];
    det=(float)determinant;memcpy(inverse,pending,36);
    if(determinant!=0) {
        cofactor[0]=(double)pending[4]*pending[8]-(double)pending[5]*pending[7];
        cofactor[1]=-((double)pending[1]*pending[8]-(double)pending[2]*pending[7]);
        cofactor[2]=(double)pending[1]*pending[5]-(double)pending[2]*pending[4];
        cofactor[3]=-((double)pending[3]*pending[8]-(double)pending[5]*pending[6]);
        cofactor[4]=(double)pending[0]*pending[8]-(double)pending[2]*pending[6];
        cofactor[5]=-((double)pending[0]*pending[5]-(double)pending[2]*pending[3]);
        cofactor[6]=(double)pending[3]*pending[7]-(double)pending[4]*pending[6];
        cofactor[7]=-((double)pending[0]*pending[7]-(double)pending[1]*pending[6]);
        cofactor[8]=(double)pending[0]*pending[4]-(double)pending[1]*pending[3];
        for(i=0;i<8;++i){minor=(float)cofactor[i];inverse[i]=(float)((double)minor/det);}
        inverse[8]=(float)(cofactor[8]/det);
    }
    for(i=0;i<3;++i) {
        float rotated=(float)(((double)local[11]*current[i*3+2]+(double)local[10]*current[i*3+1])+(double)local[9]*current[i*3]);
        value[9+i]=(float)((double)rotated+position[i]);
        for(j=0;j<3;++j) {
            const unsigned char *o=order[i*3+j];
            for(k=0;k<3;++k)terms[k]=(double)local[i*3+k]*inverse[k*3+j];
            value[i*3+j]=(float)((terms[o[0]]+terms[o[1]])+terms[o[2]]);
        }
    }
    for(i=0;i<12;++i)if(!isfinite(value[i]))return RF_FORMAT;
    memcpy(out,value,sizeof(value));return RF_OK;
}
int rf_glare_create(const rf_glare_class *classes,uint32_t count,int32_t index,
    uint32_t parent,int32_t tag,uint32_t flag,rf_object_list *list,
    const rf_glare_create_backend *backend,rf_glare_state **out)
{
    rf_glare_create_descriptor descriptor={0};rf_glare_state *state=NULL;
    const rf_glare_class *definition;float pose[12];uint32_t i;int status;
    if(!out || count>INT32_MAX)return RF_RANGE;
    if(index<0 || (uint32_t)index>=count){*out=NULL;return RF_OK;}
    if(!classes || !list || !backend || !backend->tag_pose || !backend->allocate)return RF_RANGE;
    definition=classes+index;
    if(!isfinite(definition->size_first) || !isfinite(definition->size_second))return RF_FORMAT;
    descriptor.parent=parent;
    descriptor.radius=definition->size_first>definition->size_second?definition->size_first:definition->size_second;
    if(!isfinite(descriptor.radius))return RF_FORMAT;
    status=backend->tag_pose(backend->context,parent,tag,pose);if(status)return status;
    for(i=0;i<12;++i)if(!isfinite(pose[i]))return RF_FORMAT;
    memcpy(descriptor.matrix,pose,36);memcpy(descriptor.position,pose+9,12);
    status=backend->allocate(backend->context,&descriptor,&state);if(status)return status;
    *out=state;if(!state)return RF_OK;
    state->definition=definition->definition;state->class_index=index;state->occluder=-1;
    state->cached_solid=0;state->cached_face=0;memset(state->samples,0,sizeof(state->samples));state->word_2cc=0;
    state->parent=parent;state->tag=tag;state->flags=(flag&255u)?2:0;state->active=1;
    rf_object_list_append(list,&state->link);
    for(i=0;i<3;++i)state->last_position[i]=-1000;
    state->byte_2d0=0;memset(state->vectors,0,sizeof(state->vectors));return RF_OK;
}

int rf_glare_volume_opacity(double angle_radians,float cone_degrees,float *opacity,uint32_t *draw)
{
    float value;if(!opacity || !draw)return RF_RANGE;
    if(!isfinite(angle_radians) || !isfinite(cone_degrees))return RF_FORMAT;
    value=(float)((angle_radians*(double)57.2957763671875f-cone_degrees)*(double).04f);
    if(value<-1)value=-1;if(value>1)value=1;
    value=(float)(((double)value+1.0)*.5);
    *draw=value>=0.007843137718737125f;*opacity=*draw?value:0;return RF_OK;
}
int rf_glare_volume_camera_opacity(const float position[3],const float forward[3],
    const float camera[3],float cone_degrees,float *opacity,uint32_t *draw)
{
    float delta[3],direction[3];double length,inverse,dot;unsigned i;
    if(!position || !forward || !camera || !opacity || !draw)return RF_RANGE;
    if(!isfinite(cone_degrees))return RF_FORMAT;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]) || !isfinite(forward[i]) || !isfinite(camera[i]))return RF_FORMAT;
        delta[i]=(float)((double)position[i]-camera[i]);
        if(!isfinite(delta[i]))return RF_FORMAT;
    }
    length=sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
    if(!(length>0) || !isfinite(length))return RF_FORMAT;
    inverse=1.0/length;
    for(i=0;i<3;++i)direction[i]=-(float)(inverse*delta[i]);
    dot=((double)direction[2]*forward[2]+(double)direction[1]*forward[1])+(double)direction[0]*forward[0];
    if(dot<-1 || dot>1 || !isfinite(dot))return RF_FORMAT;
    return rf_glare_volume_opacity(acos(dot),cone_degrees,opacity,draw);
}
int rf_glare_volume_actor_aim(const float local[3],const float position[3],
    const float basis[9],double *dot,uint32_t *eligible)
{
    float vector[3],direction[3];double length,inverse,value;unsigned i;
    if(!local || !position || !basis || !dot || !eligible)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(local[i]) || !isfinite(position[i]))return RF_FORMAT;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_FORMAT;
    length=sqrt(((double)local[0]*local[0]+(double)local[1]*local[1])+(double)local[2]*local[2]);
    if(length<=(double).0001f){*dot=0;*eligible=0;return RF_OK;}
    for(i=0;i<3;++i) {
        float rotated=(float)(((double)basis[6+i]*local[2]+(double)basis[3+i]*local[1])+(double)basis[i]*local[0]);
        float world=(float)((double)rotated+position[i]);vector[i]=(float)((double)world-position[i]);
        if(!isfinite(vector[i]))return RF_FORMAT;
    }
    length=sqrt(((double)vector[0]*vector[0]+(double)vector[1]*vector[1])+(double)vector[2]*vector[2]);
    if(!(length>0) || !isfinite(length))return RF_FORMAT;inverse=1.0/length;
    for(i=0;i<3;++i)direction[i]=(float)(inverse*vector[i]);
    value=((double)direction[2]*basis[8]+(double)direction[1]*basis[7])+(double)direction[0]*basis[6];
    if(!isfinite(value))return RF_FORMAT;*dot=value;*eligible=1;return RF_OK;
}
int rf_glare_volume_actor_dimensions(double aim_dot,float class_length,float class_width,
    rf_random_state *random,float dimensions[2],uint32_t *draw)
{
    rf_random_state next;uint32_t sample;float length,width;double scaled,jitter,sum;
    if(!random || !dimensions || !draw)return RF_RANGE;
    if(!isfinite(aim_dot) || !isfinite(class_length) || !isfinite(class_width))return RF_FORMAT;
    if(aim_dot<0){dimensions[0]=class_length;dimensions[1]=class_width;*draw=0;return RF_OK;}
    if(class_length==0)return RF_FORMAT;
    scaled=aim_dot*(double)class_length;length=(float)scaled;
    if(scaled<(double).3f)length=.3f;
    next=*random;rf_random_next(&next,&sample);
    jitter=((double).05f-(double)-.05f)*((double)sample/32768.0)+(double)-.05f;
    sum=jitter+(double)length;length=(float)sum;
    width=(float)(((double)class_width-(double).3f)*(sum/(double)class_length)+(double).3f);
    if(!isfinite(length) || !isfinite(width))return RF_FORMAT;
    dimensions[0]=length;dimensions[1]=width;*random=next;return RF_OK;
}
int rf_glare_volume_actor_update(uint32_t parent,uint32_t glare_flags,float length,float width,
    int (*lookup)(void *,uint32_t,const rf_glare_volume_actor **),void *context,
    rf_random_state *random,float dimensions[2],uint32_t *draw)
{
    const rf_glare_volume_actor *actor,*target=NULL;const float *aim;uint32_t handle=UINT32_MAX,i,eligible,allowed;
    rf_random_state next;float sizes[2]={length,width};double dot;int status;
    if(!lookup || !random || !dimensions || !draw)return RF_RANGE;
    if(!isfinite(length) || !isfinite(width))return RF_FORMAT;
    next=*random;allowed=*draw;status=lookup(context,parent,&actor);if(status)return status;
    if(actor && (actor->class_flags&0x800u) && !(glare_flags&2)) {
        if(actor->occupant_count>4096 || (actor->occupant_count && !actor->occupants))return RF_RANGE;
        for(i=0;i<actor->occupant_count;++i)if(actor->occupants[i]!=UINT32_MAX){handle=actor->occupants[i];break;}
        status=lookup(context,handle,&target);if(status)return status;
        aim=target && !((target->flags&8) && target->player_present)?target->command:actor->command;
        status=rf_glare_volume_actor_aim(aim,actor->position,actor->basis,&dot,&eligible);if(status)return status;
        if(!eligible)allowed=0;
        else {status=rf_glare_volume_actor_dimensions(dot,length,width,&next,sizes,&allowed);if(status)return status;}
    }
    memcpy(dimensions,sizes,sizeof(sizes));*draw=allowed;*random=next;return RF_OK;
}
int rf_glare_volume_render(rf_glare_base_owner *owner,const rf_glare_definition *definition,
    const rf_glare_volume_frame *frame,const rf_glare_volume_services *services)
{
    float opacity=0,length,width,end[3];uint32_t allowed=1,draw=0,i;int status,cleanup;
    if(!owner || !definition || !frame || !services || !services->special_allowed ||
       !services->parent_visible || !services->actor_dimensions || !services->enable ||
       !services->color || !services->texture || !services->beam)return RF_RANGE;
    if(owner->state.word_2cc) {
        status=services->special_allowed(services->context,owner,&allowed);if(status || !allowed)return status;
    }
    status=services->parent_visible(services->context,owner->parent_handle,&allowed);if(status || !allowed)return status;
    if(frame->bitmap>=0) {
        status=rf_glare_volume_camera_opacity(owner->position,owner->matrix+6,frame->camera,
            definition->cone_degrees,&opacity,&draw);if(status)return status;
    }
    length=definition->length;width=definition->height;
    if(!isfinite(length) || !isfinite(width))return RF_FORMAT;
    status=services->actor_dimensions(services->context,owner,&length,&width,&draw);if(status)return status;
    owner->radius=definition->height>definition->length?definition->height:definition->length;
    if(!draw)return RF_OK;
    if(frame->bitmap<0 || !isfinite(length) || !isfinite(width))return RF_FORMAT;
    for(i=0;i<3;++i) {
        float offset=(float)((double)owner->matrix[6+i]*length);
        end[i]=(float)((double)owner->position[i]+offset);if(!isfinite(end[i]))return RF_FORMAT;
    }
    status=services->enable(services->context,1);if(status)return status;
    status=services->color(services->context,255,255,255,(uint32_t)((double)opacity*255));
    if(!status)status=services->texture(services->context,(uint32_t)frame->bitmap,-1);
    if(!status)status=services->beam(services->context,end,owner->position,width,frame->mode);
    cleanup=services->enable(services->context,0);return status?status:cleanup;
}
int rf_glare_parent_update(rf_glare_base_owner *owner,const rf_object_registry *registry)
{
    if(!owner || !registry)return RF_RANGE;
    if(owner->parent_handle!=UINT32_MAX && !rf_object_registry_lookup(registry,owner->parent_handle))owner->flags|=2;
    return RF_OK;
}
int rf_glare_publish_tag_pose(rf_glare_base_owner *owner,const float pose[12])
{
    rf_group_attached_pose position={0};uint32_t i;int status;
    if(!owner || !pose)return RF_RANGE;
    for(i=0;i<12;++i)if(!isfinite(pose[i]))return RF_FORMAT;
    position.flags=owner->flags;position.radius=owner->body.state.bounds.radius;
    status=rf_group_pose_set_position(&position,pose+9);if(status)return status;
    memcpy(owner->position,position.public_position,12);
    memcpy(owner->body.state.position,position.position,12);memcpy(owner->body.state.next_position,position.pending,12);
    memcpy(owner->body.state.bounds.minimum,position.minimum,12);memcpy(owner->body.state.bounds.maximum,position.maximum,12);
    owner->flags=position.flags;
    if(!(owner->flags&0x100u)) {
        memcpy(owner->matrix,pose,36);memcpy(owner->body.state.orientation,pose,36);memcpy(owner->body.state.next_orientation,pose,36);
    }
    return RF_OK;
}
int rf_glare_base_close(rf_glare_base_owner **owner,rf_object_registry *registry,rf_object_list *objects)
{
    rf_glare_base_owner *v;uint32_t handle;
    if(!owner || !registry || !objects)return RF_RANGE;v=*owner;if(!v)return RF_OK;
    if(v->state.link.next || v->state.link.previous || rf_object_registry_lookup(registry,v->handle)!=&v->state)return RF_RANGE;
    handle=v->handle;rf_object_list_remove(objects,&v->object_link);rf_physics_body_close(&v->body);
    free(v);*owner=NULL;return rf_object_registry_remove(registry,handle);
}
int rf_glare_base_open(const rf_glare_create_descriptor *d,
    rf_object_registry *registry,rf_object_list *objects,uint32_t *uid_cursor,
    uint32_t parent_byte,uint32_t parent_group,const float material[3],uint32_t budget,rf_glare_base_owner **out)
{
    rf_glare_base_owner *v;rf_physics_creation_seed seed={0};uint32_t i;int status;
    if(!d || !registry || !objects || !uid_cursor || !material || !out || *out || parent_byte>255 ||
       budget<sizeof(*v) || !isfinite(d->radius) || !objects->sentinel.next || !objects->sentinel.previous ||
       objects->sentinel.next->previous!=&objects->sentinel || objects->sentinel.previous->next!=&objects->sentinel)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(d->position[i]) || !isfinite(material[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(d->matrix[i]))return RF_RANGE;
    if(!registry->count){*out=NULL;return RF_OK;}
    v=calloc(1,sizeof(*v));if(!v)return RF_IO;
    v->state.parent=UINT32_MAX;v->state.tag=-1;v->flags=0x6030000;v->kind=10;v->identifier=-1;
    v->parent_handle=d->parent;v->parent_byte=parent_byte;v->parent_group=parent_group;v->health=100;
    v->radius=d->radius<=0?1:d->radius;memcpy(v->position,d->position,12);memcpy(v->matrix,d->matrix,36);
    rf_object_list_append(objects,&v->object_link);status=rf_object_registry_insert(registry,&v->state,&v->handle);
    if(status){rf_object_list_remove(objects,&v->object_link);free(v);return status;}
    v->uid=(*uid_cursor)--;seed.radius=d->radius<0?v->radius:d->radius;seed.word_14=0x3f800000;
    memcpy(seed.position,d->position,12);memcpy(seed.basis,d->matrix,36);
    status=rf_physics_creation_body_open(&seed,material[0],material[1],material[2],sizeof(v->body),&v->body);
    if(status){(void)rf_glare_base_close(&v,registry,objects);return status;}
    v->allocated_bytes=sizeof(*v);*out=v;return RF_OK;
}

typedef struct glare_owned_context {
    rf_object_registry *registry;rf_object_list *objects;uint32_t *uid;
    uint32_t parent_byte,parent_group,budget;const float *material;
    const rf_glare_services *services;rf_glare_base_owner *owner;
} glare_owned_context;
static int glare_owned_pose(void *context,uint32_t parent,int32_t tag,float pose[12])
{glare_owned_context *c=context;return c->services->tag_pose(c->services->context,parent,tag,pose);}
static int glare_owned_allocate(void *context,const rf_glare_create_descriptor *d,rf_glare_state **out)
{
    glare_owned_context *c=context;int status=rf_glare_base_open(d,c->registry,c->objects,c->uid,
        c->parent_byte,c->parent_group,c->material,c->budget,&c->owner);
    if(!status)*out=c->owner?&c->owner->state:NULL;return status;
}
int rf_glare_owned_open(const rf_glare_class *classes,uint32_t count,int32_t index,
    uint32_t parent,int32_t tag,uint32_t flag,rf_object_registry *registry,
    rf_object_list *objects,rf_object_list *glares,uint32_t *uid_cursor,
    uint32_t parent_byte,uint32_t parent_group,const float material[3],uint32_t budget,
    const rf_glare_services *services,rf_glare_base_owner **out)
{
    glare_owned_context c={registry,objects,uid_cursor,parent_byte,parent_group,budget,material,services,NULL};
    rf_glare_create_backend backend={glare_owned_pose,glare_owned_allocate,&c};rf_glare_state *state=NULL;int status;
    if(!out || *out || !registry || !objects || !glares || !uid_cursor || !material || !services || !services->tag_pose ||
       !glares->sentinel.next || !glares->sentinel.previous || glares->sentinel.next->previous!=&glares->sentinel || glares->sentinel.previous->next!=&glares->sentinel)return RF_RANGE;
    status=rf_glare_create(classes,count,index,parent,tag,flag,glares,&backend,&state);if(status)return status;
    *out=c.owner;return RF_OK;
}
int rf_glare_owned_close(rf_glare_base_owner **owner,rf_object_registry *registry,
    rf_object_list *objects,rf_object_list *glares)
{
    rf_object_link *node;uint32_t i;
    if(!owner || !registry || !objects || !glares)return RF_RANGE;if(!*owner)return RF_OK;
    if(rf_object_registry_lookup(registry,(*owner)->handle)!=&(*owner)->state)return RF_RANGE;
    node=glares->sentinel.next;
    for(i=0;i<glares->count && node && node!=&glares->sentinel;++i,node=node->next)if(node==&(*owner)->state.link)break;
    if(i==glares->count || node!=&(*owner)->state.link || !node->previous || !node->next || node->previous->next!=node || node->next->previous!=node)return RF_RANGE;
    rf_object_list_remove(glares,node);return rf_glare_base_close(owner,registry,objects);
}

int rf_object_model_attach(rf_object_model_attachment *state,const char *name,
    uint32_t kind,const rf_object_model_backend *backend)
{
    char stem[64],*dot;size_t length;uint32_t model;float center[3]={0};int status;
    if(!state || !name || !backend || !backend->load || !backend->bounds ||
       !backend->animate || !backend->property)return RF_RANGE;
    for(length=0;length<sizeof(stem) && name[length];++length){}
    if(length==sizeof(stem))return RF_RANGE;
    memcpy(stem,name,length+1);dot=strrchr(stem,'.');if(dot)*dot=0;
    if(kind>=1 && kind<=3) {
        status=backend->load(backend->context,kind,kind==2?stem:name,
            kind==1?1:kind==3?9999999:0,kind==1?UINT32_MAX:0,&model);
        if(status)return status;
        state->model=model;
    }
    state->model_index=-1;if(!state->model)return RF_OK;
    status=backend->bounds(backend->context,state->model,center,&state->radius);if(status)return status;
    if(!isfinite(center[0]) || !isfinite(center[1]) || !isfinite(center[2]) || !isfinite(state->radius))return RF_RANGE;
    state->radius=(float)(sqrt(((double)center[0]*center[0]+(double)center[1]*center[1])+(double)center[2]*center[2])+(double)state->radius);
    if(!isfinite(state->radius))return RF_RANGE;
    if(kind==3){status=backend->animate(backend->context,state->model,0,1);if(status)return status;}
    return backend->property(backend->context,state->model,&state->model_property);
}
int rf_clutter_base_close(rf_clutter_base_owner **owner,rf_object_registry *registry,
    rf_object_list *objects,const rf_clutter_base_backend *backend)
{
    rf_clutter_base_owner *v;uint32_t handle;
    if(!owner || !registry || !objects || !backend || !backend->release)return RF_RANGE;
    v=*owner;if(!v)return RF_OK;
    if(v->state.link.next || v->state.link.previous || !objects->count ||
       !v->object_link.next || !v->object_link.previous ||
       v->object_link.next->previous!=&v->object_link || v->object_link.previous->next!=&v->object_link ||
       rf_object_registry_lookup(registry,v->state.handle)!=&v->state)return RF_RANGE;
    handle=v->state.handle;rf_object_list_remove(objects,&v->object_link);
    rf_physics_body_close(&v->body);
    if(v->attachment.model)backend->release(backend->model.context,v->attachment.model);
    free(v);*owner=NULL;return rf_object_registry_remove(registry,handle);
}
typedef struct clutter_base_model_context {rf_clutter_base_owner *owner;const rf_clutter_base_backend *backend;} clutter_base_model_context;
static int clutter_base_model_load(void *context,uint32_t kind,const char *name,uint32_t first,uint32_t second,uint32_t *model)
{clutter_base_model_context *c=context;int status=c->backend->model.load(c->backend->model.context,kind,name,first,second,model);if(!status)c->owner->state.model=*model;return status;}
static int clutter_base_model_bounds(void *context,uint32_t model,float center[3],float *radius)
{clutter_base_model_context *c=context;return c->backend->model.bounds(c->backend->model.context,model,center,radius);}
static int clutter_base_model_animate(void *context,uint32_t model,int32_t motion,float speed)
{clutter_base_model_context *c=context;return c->backend->model.animate(c->backend->model.context,model,motion,speed);}
static int clutter_base_model_property(void *context,uint32_t model,int32_t *property)
{clutter_base_model_context *c=context;return c->backend->model.property(c->backend->model.context,model,property);}
int rf_clutter_base_open(const rf_clutter_create_descriptor *d,
    rf_object_registry *registry,rf_object_list *objects,uint32_t *uid_cursor,
    uint32_t room,uint32_t parent_byte,uint32_t parent_group,const float material[3],
    const rf_clutter_base_backend *backend,uint32_t budget,rf_clutter_base_owner **out)
{
    rf_clutter_base_owner *v;rf_physics_creation_seed seed={0};rf_clutter_model_view view={0};
    rf_physics_sphere *scratch=NULL;uint64_t peak,retained;uint32_t i;int status;
    clutter_base_model_context context;
    rf_object_model_backend model_backend={clutter_base_model_load,clutter_base_model_bounds,clutter_base_model_animate,clutter_base_model_property,&context};
    if(!d || !registry || !objects || !uid_cursor || !material || !backend || !out || *out ||
       !backend->model.load || !backend->model.bounds || !backend->model.animate || !backend->model.property ||
       !backend->spheres || !backend->release || parent_byte>255 || !isfinite(d->radius) ||
       budget<sizeof(*v) || !objects->sentinel.next || !objects->sentinel.previous ||
       objects->sentinel.next->previous!=&objects->sentinel || objects->sentinel.previous->next!=&objects->sentinel)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(d->position[i]) || !isfinite(material[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(d->matrix[i]))return RF_RANGE;
    if(!registry->count)return RF_OK;
    v=calloc(1,sizeof(*v));if(!v)return RF_IO;
    v->state.token=(uint32_t)(uintptr_t)&v->state;v->state.health=100;
    v->state.flags=d->allocation_flags|0x6000000u;if(d->allocation_flags&0x4000)v->state.flags|=0x8000;
    v->material=d->material;v->identifier=d->identifier;v->parent_byte=parent_byte;v->parent_group=parent_group;
    memcpy(v->state.position,d->position,12);memcpy(v->matrix,d->matrix,36);
    v->attachment.model_index=-1;v->attachment.model_property=-1;
    rf_object_list_append(objects,&v->object_link);
    status=rf_object_registry_insert(registry,&v->state,&v->state.handle);
    if(status){rf_object_list_remove(objects,&v->object_link);free(v);return status;}
    v->uid=(*uid_cursor)--;
    if(d->model) {
        context.owner=v;context.backend=backend;
        status=rf_object_model_attach(&v->attachment,d->model,d->kind,&model_backend);if(status)goto failed;
        if(!v->attachment.model){status=RF_OK;goto failed;}
        status=backend->spheres(backend->model.context,v->attachment.model,&view);if(status)goto failed;
    } else v->attachment.radius=d->radius<=0?1:d->radius;
    seed.flags=d->flags;if(d->allocation_flags&0x10000)seed.flags&=~0x20u;
    seed.radius=d->radius<0?v->attachment.radius:d->radius;
    memcpy(seed.position,d->position,12);memcpy(seed.basis,d->matrix,36);
    retained=sizeof(*v)+(uint64_t)((seed.flags&0x70)?(view.count?view.count:1):0)*sizeof(*scratch);
    peak=retained+(uint64_t)view.count*sizeof(*scratch);
    if(peak>budget){status=RF_RANGE;goto failed;}
    if(view.count) {
        scratch=calloc(view.count,sizeof(*scratch));if(!scratch){status=RF_IO;goto failed;}
        status=rf_model_creation_spheres(view.spheres,view.count,view.wrapper_kind,view.matrices,view.bones,scratch,view.count);
        if(status)goto failed;
    }
    seed.spheres=scratch;seed.sphere_count=view.count;
    status=rf_physics_creation_body_open(&seed,material[0],material[1],material[2],
        budget-(uint32_t)sizeof(*v)-(uint32_t)(view.count*sizeof(*scratch))+sizeof(v->body),&v->body);
    if(status)goto failed;
    free(scratch);scratch=NULL;v->state.model=v->attachment.model;v->state.physics_flags=v->body.state.flags;
    v->state.flags|=0x400000;v->state.first_word=room;memcpy(v->query_position,v->state.position,12);
    v->allocated_bytes=(uint32_t)retained;v->peak_bytes=(uint32_t)peak;*out=v;return RF_OK;
failed:
    free(scratch);(void)rf_clutter_base_close(&v,registry,objects,backend);return status;
}
typedef struct clutter_static_context {rf_vpp *models;const rf_clutter_create_descriptor *descriptor;uint32_t budget;} clutter_static_context;
static int clutter_static_load(void *context,uint32_t kind,const char *name,uint32_t first,uint32_t second,uint32_t *model)
{
    clutter_static_context *c=context;rf_static_model_metadata *metadata;int status;
    if(kind!=1 || first!=1 || second!=UINT32_MAX || c->budget<sizeof(rf_clutter_base_owner)+sizeof(*metadata))return RF_RANGE;
    metadata=calloc(1,sizeof(*metadata));if(!metadata)return RF_IO;
    status=rf_static_model_metadata_open(c->models,name,c->budget-sizeof(rf_clutter_base_owner),metadata);
    if(status){rf_static_model_metadata_close(metadata);free(metadata);if(status==RF_NOT_FOUND){*model=0;return RF_OK;}return status;}
    *model=(uint32_t)(uintptr_t)metadata;return RF_OK;
}
static int clutter_static_bounds(void *context,uint32_t model,float center[3],float *radius)
{const rf_static_model_metadata *m=(const rf_static_model_metadata *)(uintptr_t)model;(void)context;memcpy(center,m->bound,12);*radius=m->bound[3];return RF_OK;}
static int clutter_static_animate(void *context,uint32_t model,int32_t motion,float speed)
{(void)context;(void)model;(void)motion;(void)speed;return RF_FORMAT;}
static int clutter_static_property(void *context,uint32_t model,int32_t *property)
{(void)context;(void)model;*property=-1;return RF_OK;}
static int clutter_static_spheres(void *context,uint32_t model,rf_clutter_model_view *view)
{
    clutter_static_context *c=context;const rf_static_model_metadata *m=(const rf_static_model_metadata *)(uintptr_t)model;
    uint32_t flags=c->descriptor->flags;uint64_t peak;
    if(c->descriptor->allocation_flags&0x10000)flags&=~0x20u;
    peak=sizeof(rf_clutter_base_owner)+(uint64_t)m->allocated_bytes+(uint64_t)m->count*sizeof(rf_physics_sphere)+
        (uint64_t)((flags&0x70)?(m->count?m->count:1):0)*sizeof(rf_physics_sphere);
    if(peak>c->budget)return RF_RANGE;
    view->spheres=m->spheres;view->count=m->count;view->wrapper_kind=1;view->matrices=NULL;view->bones=0;return RF_OK;
}
static void clutter_static_release(void *context,uint32_t model)
{rf_static_model_metadata *m=(rf_static_model_metadata *)(uintptr_t)model;(void)context;rf_static_model_metadata_close(m);free(m);}
int rf_clutter_static_base_open(rf_vpp *models,const rf_clutter_create_descriptor *descriptor,
    rf_object_registry *registry,rf_object_list *objects,uint32_t *uid_cursor,
    uint32_t room,uint32_t parent_byte,uint32_t parent_group,const float material[3],uint32_t budget,rf_clutter_base_owner **out)
{
    clutter_static_context context={models,descriptor,budget};rf_clutter_base_owner *v=NULL;int status;uint32_t peak;
    rf_clutter_base_backend backend={{clutter_static_load,clutter_static_bounds,clutter_static_animate,clutter_static_property,&context},clutter_static_spheres,clutter_static_release};
    if(!models || !descriptor || descriptor->kind!=1 || !descriptor->model || !out || *out)return RF_RANGE;
    status=rf_clutter_base_open(descriptor,registry,objects,uid_cursor,room,parent_byte,parent_group,material,&backend,budget,&v);
    if(status)return status;
    if(v) {
        const rf_static_model_metadata *m=(const rf_static_model_metadata *)(uintptr_t)v->attachment.model;
        peak=sizeof(*v)+m->peak_bytes;v->allocated_bytes+=m->allocated_bytes;v->peak_bytes+=m->allocated_bytes;
        if(v->peak_bytes<peak)v->peak_bytes=peak;
    }
    *out=v;return RF_OK;
}
int rf_clutter_static_base_close(rf_clutter_base_owner **owner,rf_object_registry *registry,rf_object_list *objects)
{
    rf_clutter_base_backend backend={0};backend.release=clutter_static_release;
    return rf_clutter_base_close(owner,registry,objects,&backend);
}
static int clutter_shared_load(void *context,uint32_t kind,const char *name,uint32_t first,uint32_t second,uint32_t *model)
{
    rf_clutter_shared_static_model *shared=context;char compiled[64];int status;
    if(kind!=1 || first!=1 || second!=UINT32_MAX)return RF_RANGE;
    status=rf_model_compiled_filename(name,compiled,".v3m");if(status)return status;
    if(rf_emitter_name_lookup(&shared->filename,1,compiled)!=0){*model=0;return RF_OK;}
    if(shared->references==UINT32_MAX)return RF_RANGE;
    ++shared->references;*model=(uint32_t)(uintptr_t)shared;return RF_OK;
}
static int clutter_shared_bounds(void *context,uint32_t model,float center[3],float *radius)
{
    const rf_clutter_shared_static_model *shared=context;
    if(model!=(uint32_t)(uintptr_t)shared)return RF_RANGE;
    memcpy(center,shared->resource->bound,12);*radius=shared->resource->bound[3];return RF_OK;
}
static int clutter_shared_spheres(void *context,uint32_t model,rf_clutter_model_view *view)
{
    const rf_clutter_shared_static_model *shared=context;
    if(model!=(uint32_t)(uintptr_t)shared)return RF_RANGE;
    view->spheres=shared->resource->spheres;view->count=shared->resource->sphere_count;
    view->wrapper_kind=1;view->matrices=NULL;view->bones=0;return RF_OK;
}
static void clutter_shared_release(void *context,uint32_t model)
{
    rf_clutter_shared_static_model *shared=context;
    if(model==(uint32_t)(uintptr_t)shared && shared->references)--shared->references;
}
int rf_clutter_shared_static_base_open(rf_clutter_shared_static_model *model,
    const rf_clutter_create_descriptor *descriptor,rf_object_registry *registry,
    rf_object_list *objects,uint32_t *uid_cursor,uint32_t room,uint32_t parent_byte,
    uint32_t parent_group,const float material[3],uint32_t budget,rf_clutter_base_owner **out)
{
    rf_clutter_base_backend backend={{clutter_shared_load,clutter_shared_bounds,clutter_static_animate,clutter_static_property,model},clutter_shared_spheres,clutter_shared_release};
    if(!model || !model->filename || !model->resource || !descriptor || descriptor->kind!=1 || !descriptor->model ||
       (model->resource->sphere_count && !model->resource->spheres))return RF_RANGE;
    return rf_clutter_base_open(descriptor,registry,objects,uid_cursor,room,parent_byte,parent_group,material,&backend,budget,out);
}
int rf_clutter_shared_static_base_close(rf_clutter_shared_static_model *model,
    rf_clutter_base_owner **owner,rf_object_registry *registry,rf_object_list *objects)
{
    rf_clutter_base_backend backend={0};backend.model.context=model;backend.release=clutter_shared_release;
    if(!model || !owner || (*owner && ((*owner)->attachment.model!=(uint32_t)(uintptr_t)model || !model->references)))return RF_RANGE;
    return rf_clutter_base_close(owner,registry,objects,&backend);
}
static int clutter_skin_name_equal(const char *first,const char *second)
{
    unsigned char a,b;
    do {
        a=(unsigned char)*first++;b=(unsigned char)*second++;
        if(a>='A' && a<='Z')a=(unsigned char)(a+'a'-'A');
        if(b>='A' && b<='Z')b=(unsigned char)(b+'a'-'A');
        if(a!=b)return 0;
    } while(a);
    return 1;
}
uint32_t rf_clutter_material_index(const char *name)
{
    static const char *const names[]={"Default","Rock","Metal","Flesh","Water","Lava","Solid","Sand","Ice","Glass"};
    uint32_t i;if(!name)return 0;
    for(i=0;i<10;++i)if(clutter_skin_name_equal(names[i],name))return i;
    return 0;
}
int rf_glare_corona_render(rf_glare_base_owner *owner,const rf_glare_definition *definition,
    const rf_glare_corona_frame *frame,const rf_glare_corona_services *services)
{
    rf_glare_corona_tail tail={0};rf_glare_corona_environment environment;rf_glare_corona_values values;
    float camera_values[6],fade[2]={1,1};double angle,flash_alpha;uint32_t visible,allowed;int status;
    if(!owner || !definition || !frame || frame->view>1 || !services || !services->parent_visible ||
        !services->search || !services->special_visible || !services->flash)return RF_RANGE;
    if(frame->bitmap==-1)return RF_OK;if(frame->bitmap<0)return RF_RANGE;
    status=services->parent_visible(services->context,owner->parent_handle,&allowed);if(status || !allowed)return status;
    status=rf_glare_corona_camera_setup(owner,frame->camera,frame->basis,camera_values,&angle);if(status)return status;
    tail.bitmap=(uint32_t)frame->bitmap;tail.view=frame->view;tail.side_dot=camera_values[5];
    visible=owner->state.active!=0;
    if(visible) {
        if(owner->state.word_2cc)status=services->special_visible(services->context,owner,frame->camera,&visible);
        else status=rf_glare_refresh_visibility(owner,frame->camera,frame->frame,frame->face_cache_state,services->search,services->context,&visible);
        if(status)return status;
    }
    if(!visible) {
        status=rf_glare_fade_samples(&owner->state,frame->view,fade,&tail.draw);if(status)return status;
        tail.intensity=fade[0];tail.size=fade[1];
    } else {
        environment=(rf_glare_corona_environment){camera_values[4],camera_values[3],frame->field_of_view,frame->intensity_scale,frame->size_scale};
        status=rf_glare_corona_attenuate(&owner->state,frame->view,definition,&environment,angle,&values);if(status)return status;
        tail.intensity=values.intensity;tail.size=values.size;tail.angular=values.angular;tail.draw=1;
        if(values.flash>0 && (uint8_t)definition->color[0] && !owner->state.word_2cc) {
            flash_alpha=((double)values.flash*definition->intensity)*64.0;
            if(!isfinite(flash_alpha) || flash_alpha < -2147483648.0 || flash_alpha>=2147483648.0)return RF_FORMAT;
            status=services->flash(services->context,(uint8_t)definition->color[0],(uint8_t)definition->color[1],
                (uint8_t)definition->color[2],(int32_t)flash_alpha);if(status)return status;
        }
    }
    return rf_glare_corona_submit(owner,&tail,&services->graphics);
}
static double corona_dot(const float first[3],const float second[3])
{
    return ((double)first[2]*second[2]+(double)first[1]*second[1])+(double)first[0]*second[0];
}
int rf_glare_corona_camera_setup(const rf_glare_base_owner *owner,const float camera[3],
    const float basis[9],float values[6],double *view_angle)
{
    float delta[3],direction[3],reverse[3],out[6];double length,reciprocal,dot,angle,side;uint32_t i;
    if(!owner || !camera || !basis || !values || !view_angle)return RF_RANGE;
    for(i=0;i<3;++i) {
        volatile float component=owner->position[i]-camera[i];
        if(!isfinite(owner->position[i]) || !isfinite(camera[i]) || !isfinite(component))return RF_FORMAT;
        delta[i]=component;
    }
    for(i=0;i<9;++i)if(!isfinite(basis[i]) || !isfinite(owner->matrix[i]))return RF_FORMAT;
    length=sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
    if(!isfinite(length) || length<=0)return RF_FORMAT;reciprocal=1.0/length;
    for(i=0;i<3;++i){direction[i]=(float)(reciprocal*(double)delta[i]);reverse[i]=-direction[i];out[i]=direction[i];}
    dot=corona_dot(owner->matrix+6,reverse);if(dot < -1 || dot > 1)return RF_FORMAT;
    out[3]=(float)(acos(dot)*(double)57.2957763671875f);out[4]=(float)length;
    dot=corona_dot(basis+6,direction);if(dot < -1 || dot > 1)return RF_FORMAT;angle=acos(dot);
    side=corona_dot(basis,direction);out[5]=side<0?-1.0f:side>0?1.0f:0;
    if(!isfinite(out[3]) || !isfinite(out[4]) || !isfinite(angle))return RF_FORMAT;
    memcpy(values,out,sizeof(out));*view_angle=angle;return RF_OK;
}
int rf_glare_corona_attenuate(const rf_glare_state *state,uint32_t view,
    const rf_glare_definition *definition,const rf_glare_corona_environment *environment,
    double view_angle_radians,rf_glare_corona_values *result)
{
    rf_glare_corona_values value;double angular,factor,size;volatile float half,rounded_factor,intensity,flash,cone_factor;
    if(!state || view>1 || !definition || !environment || !result)return RF_RANGE;
    if(!isfinite(view_angle_radians) || !isfinite(environment->distance) || environment->distance<=0 ||
        !isfinite(environment->glare_angle) || !isfinite(environment->field_of_view) || environment->field_of_view<=0 ||
        !isfinite(environment->intensity_scale) || !isfinite(environment->size_scale) ||
        !isfinite(definition->cone_degrees) || definition->cone_degrees<=0 || !isfinite(definition->intensity) ||
        !isfinite(definition->radius_distance) || !isfinite(definition->radius_scale) || !isfinite(definition->diminish) ||
        !isfinite(state->samples[view]) || !isfinite(state->samples[2+view]))return RF_FORMAT;
    half=environment->field_of_view*.5f;
    angular=((double)half-view_angle_radians*(double)57.2957763671875f)/(double)half;
    value.angular=(float)angular;factor=angular+1.0;rounded_factor=(float)factor;
    intensity=(float)((factor*(double)environment->intensity_scale)*(40.0/(double)environment->distance));
    if(environment->glare_angle>definition->cone_degrees)
        intensity=(float)((1.0-((double)environment->glare_angle-definition->cone_degrees)*(double).1f)*(double)intensity);
    intensity=(float)((double)intensity*definition->intensity);
    if(intensity<0)intensity=0;if(intensity>1)intensity=1;
    value.intensity=(float)(((double)intensity+state->samples[view])*.5);
    size=((sqrt((double)environment->distance)*definition->radius_scale-definition->diminish)*definition->radius_distance)*
        ((double)rounded_factor*environment->size_scale);
    if(environment->glare_angle>definition->cone_degrees)
        size*=1.0-((double)environment->glare_angle-definition->cone_degrees)*(double).05f;
    if(size<0)size=0;value.size=(float)((size+state->samples[2+view])*.5);
    flash=(float)(((12.0-environment->distance)/environment->distance)*definition->intensity);
    flash=(float)((double)(value.angular>0?value.angular:0)*(double)flash);
    cone_factor=(float)(((double)definition->cone_degrees-environment->glare_angle)/definition->cone_degrees);
    flash=(float)((double)(cone_factor>0?cone_factor:0)*(double)flash);
    if(flash<0)flash=0;if(flash>1)flash=1;value.flash=(float)((double)flash*(double)flash);
    if(!isfinite(value.intensity) || !isfinite(value.size) || !isfinite(value.angular) || !isfinite(value.flash))return RF_FORMAT;
    *result=value;return RF_OK;
}
int rf_glare_corona_submit(rf_glare_base_owner *owner,const rf_glare_corona_tail *tail,
    const rf_glare_corona_backend *backend)
{
    const uint32_t mode=1u|(2u<<5)|(3u<<10)|(2u<<15)|(3u<<25);uint32_t alpha;volatile float angle;int status;
    if(!owner || !tail || !backend || tail->view>1 || !backend->color ||
        !backend->texture || !backend->billboard || !backend->oriented)return RF_RANGE;
    if(!tail->draw)return RF_OK;
    if(!isfinite(tail->intensity) || tail->intensity<0 || tail->intensity>1 || !isfinite(tail->size) ||
        !isfinite(tail->angular) || !isfinite(tail->side_dot) || !isfinite(owner->radius))return RF_FORMAT;
    alpha=(uint32_t)((double)tail->intensity*255.0);
    owner->state.samples[tail->view]=tail->intensity;owner->state.samples[2+tail->view]=tail->size;
    owner->radius=tail->size>owner->radius?tail->size:owner->radius;
    angle=(float)((1.0-(double)tail->angular)*.5);if(tail->side_dot<0)angle=-angle;
    status=backend->color(backend->context,255,255,255,alpha);if(status)return status;
    status=backend->texture(backend->context,tail->bitmap,-1);if(status)return status;
    if(owner->state.byte_2d0)return backend->oriented(backend->context,owner->state.vectors[0],owner->state.vectors[1],tail->size,mode);
    return backend->billboard(backend->context,owner->position,angle,tail->size,mode);
}
int rf_glare_fade_samples(rf_glare_state *state,uint32_t view,float values[2],uint32_t *draw)
{
    float first,second;
    if(!state || view>1 || !values || !draw)return RF_RANGE;
    first=state->samples[view];second=state->samples[2+view];
    if(first>0 && second>0) {
        volatile float intensity=first*.8f,size=second*.6f;
        values[0]=intensity;values[1]=size;
        if(intensity>=.05f && size>=.05f){*draw=1;return RF_OK;}
        state->samples[view]=0;state->samples[2+view]=0;
    }
    *draw=0;return RF_OK;
}
int rf_glare_refresh_visibility(rf_glare_base_owner *owner,const float camera[3],
    uint32_t frame,int32_t face_cache_state,
    int (*search)(void *,rf_glare_base_owner *,const float[3],uint32_t *),void *context,uint32_t *visible)
{
    uint32_t result;int status;
    if(!owner || !camera || !search || !visible)return RF_RANGE;
    if(!owner->state.active || owner->state.word_2cc)return RF_RANGE;
    if(face_cache_state>=0)owner->state.cached_face=0;
    if(!((owner->handle^frame)&1)) {
        status=search(context,owner,camera,&result);if(status)return status;
        owner->state.reserved[0]=(uint8_t)result;
    }
    *visible=owner->state.reserved[0]!=0;return RF_OK;
}
int rf_glare_occluder_test(const rf_collision_visibility_object *candidate,
    uint32_t candidate_handle,uint32_t excluded,const rf_glare_base_owner *glare,
    const float camera[3],const rf_collision_visibility_backend *backend,uint32_t *blocked)
{
    rf_collision_model_part_query query={0};rf_collision_model_response_hit hit={0};float point[3];
    uint32_t accepted,i;int status;
    if(!candidate || !glare || !camera || !backend || !backend->model || !blocked)return RF_RANGE;
    if(!(candidate->flags&0x10) || !candidate->model || candidate->token==excluded || candidate_handle==glare->parent_handle){*blocked=0;return RF_OK;}
    status=rf_collision_segment_box(candidate->minimum,candidate->maximum,glare->position,camera,point,&accepted);if(status)return status;
    if(!accepted){*blocked=0;return RF_OK;}
    memcpy(query.input.origin,candidate->position,12);memcpy(query.input.matrix,candidate->matrix,36);
    memcpy(query.input.start,camera,12);query.input.flags=1;
    for(i=0;i<3;++i) {
        if(!isfinite(candidate->position[i]))return RF_FORMAT;
        query.input.displacement[i]=glare->position[i]-camera[i];if(!isfinite(query.input.displacement[i]))return RF_FORMAT;
    }
    for(i=0;i<9;++i)if(!isfinite(candidate->matrix[i]))return RF_FORMAT;
    status=backend->model(backend->context,candidate,&query,&hit,1,&accepted);if(status)return status;
    *blocked=!!(accepted&255);return RF_OK;
}
static void glare_solid_query_init(rf_glare_solid_query *query,uint32_t special)
{
    memset(query,0,sizeof(*query));query->input.matrix[0]=1;query->input.matrix[4]=1;query->input.matrix[8]=1;
    query->input.flags=(special&255)?0x85:5;
}
static int glare_moving_solid_test(const rf_glare_visibility_object *object,const float camera[3],
    const float position[3],rf_glare_solid_query *query,const rf_glare_visibility_backend *backend,uint32_t *blocked)
{
    float point[3],start[3],end[3],local_end[3];uint32_t accepted,i;int status;
    rf_collision_solid_response_hit hit={0};
    *blocked=0;if(!(object->geometry.flags&0x10))return RF_OK;
    status=rf_collision_segment_box(object->geometry.minimum,object->geometry.maximum,position,camera,point,&accepted);
    if(status || !accepted)return status;
    for(i=0;i<3;++i){start[i]=camera[i]-object->geometry.position[i];end[i]=position[i]-object->geometry.position[i];}
    for(i=0;i<3;++i) {
        const float *m=object->geometry.matrix+3*i;
        query->input.start[i]=(start[2]*m[2]+start[1]*m[1])+start[0]*m[0];
        local_end[i]=(end[2]*m[2]+end[1]*m[1])+end[0]*m[0];
        query->input.displacement[i]=local_end[i]-query->input.start[i];
        if(!isfinite(query->input.start[i]) || !isfinite(query->input.displacement[i]))return RF_FORMAT;
    }
    status=backend->solid_query(backend->context,object->solid,query,&hit,1);if(status)return status;
    *blocked=hit.count>0;return RF_OK;
}
int rf_glare_visibility_search(rf_glare_base_owner *glare,const float camera[3],
    const rf_glare_visibility_list *movers,const rf_glare_visibility_list *actors,
    const rf_glare_visibility_object *selected,uint32_t world,uint32_t special,
    const rf_glare_visibility_backend *backend,uint32_t *visible)
{
    const rf_glare_visibility_object *object=NULL,*associated=NULL;rf_glare_solid_query query;
    rf_collision_solid_response_hit hit={0};rf_collision_visibility_backend collision;
    uint32_t blocked,i,selected_word0,candidate_word0,value,excluded=selected?selected->geometry.token:0;int status;
    if(!glare || !camera || !movers || !actors || !visible || !backend || !backend->lookup ||
       !backend->solid_owner || !backend->object_word0 || !backend->state || !backend->associated ||
       !backend->solid_query || !backend->model || (movers->count && !movers->items) ||
       (actors->count && !actors->items))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(camera[i]) || !isfinite(glare->position[i]))return RF_FORMAT;
    collision.model=backend->model;collision.world=NULL;collision.context=backend->context;
    if(glare->state.occluder!=-1) {
        status=backend->lookup(backend->context,(uint32_t)glare->state.occluder,&object);if(status)return status;
        if(object) {
            status=rf_glare_occluder_test(&object->geometry,object->handle,excluded,glare,camera,&collision,&blocked);if(status)return status;
            if(blocked){*visible=0;return RF_OK;}
        }
        glare->state.occluder=-1;
    }
    glare_solid_query_init(&query,special);
    if(glare->state.cached_solid) {
        object=NULL;status=backend->solid_owner(backend->context,glare->state.cached_solid,&object);if(status)return status;
        if(object) {
            status=glare_moving_solid_test(object,camera,glare->position,&query,backend,&blocked);if(status)return status;
            if(blocked){*visible=0;return RF_OK;}
        }
        glare->state.cached_solid=0;
    }
    glare_solid_query_init(&query,special);query.preferred_face=glare->state.cached_face;
    for(i=0;i<3;++i) {
        query.input.start[i]=camera[i];query.input.displacement[i]=glare->position[i]-camera[i];
        if(!isfinite(query.input.displacement[i]))return RF_FORMAT;
    }
    status=backend->solid_query(backend->context,world,&query,&hit,1);if(status)return status;
    if(hit.count>0){glare->state.cached_face=hit.face;*visible=0;return RF_OK;}
    for(i=0;i<movers->count;++i) {
        object=movers->items+i;
        status=glare_moving_solid_test(object,camera,glare->position,&query,backend,&blocked);if(status)return status;
        if(blocked){glare->state.cached_solid=object->geometry.token;*visible=0;return RF_OK;}
    }
    if(selected) {
        status=backend->object_word0(backend->context,selected,&selected_word0);if(status)return status;
        for(i=0;i<actors->count;++i) {
            object=actors->items+i;
            status=backend->object_word0(backend->context,object,&candidate_word0);if(status)return status;if(selected_word0!=candidate_word0)continue;
            status=backend->state(backend->context,object,&value);if(status)return status;if(value&255)continue;
            status=backend->state(backend->context,selected,&value);if(status)return status;
            if((value&255)==1) {
                associated=NULL;status=backend->associated(backend->context,selected,&associated);if(status)return status;
                if(associated && associated->geometry.token==object->geometry.token)continue;
            }
            status=rf_glare_occluder_test(&object->geometry,object->handle,excluded,glare,camera,&collision,&blocked);if(status)return status;
            if(blocked){glare->state.occluder=(int32_t)object->handle;*visible=0;return RF_OK;}
        }
    }
    object=NULL;status=backend->lookup(backend->context,glare->parent_handle,&object);if(status)return status;
    if(object) {
        status=rf_glare_occluder_test(&object->geometry,object->handle,excluded,glare,camera,&collision,&blocked);if(status)return status;
        if(blocked){glare->state.occluder=(int32_t)object->handle;*visible=0;return RF_OK;}
    }
    *visible=1;return RF_OK;
}
int rf_glare_collect(rf_glare_base_owner *owner,uint32_t room,uint32_t current_room,
    int32_t volume,uint32_t callback,const rf_visibility_frustum *frustum,
    const float cull_position[3],rf_render_queue_record *records,uint32_t capacity,
    uint32_t *count,uint32_t *accepted)
{
    rf_render_queue_record entry={0};uint32_t visible;int status;
    if(!owner || !accepted)return RF_RANGE;
    if(room!=current_room || !owner->state.active){*accepted=0;return RF_OK;}
    if(volume>0 && !callback)return RF_RANGE;
    entry.object=owner->handle;memcpy(entry.position,owner->position,12);entry.radius=owner->radius;
    entry.sorted=volume>0;entry.lighting_flag=1;entry.callback=volume>0?callback:0;
    status=rf_render_queue_append(frustum,cull_position,&entry,records,capacity,count,&visible);if(status)return status;
    if(visible)owner->state.flags|=0x80000000u;*accepted=visible;return RF_OK;
}
int rf_glare_render_pass(rf_object_list *list,const void *const *views,uint32_t count,
    const void *current,uint32_t reflections,const rf_glare_render_backend *backend)
{
    rf_object_link *node;uint32_t view,i;int status,disabled;
    if(!list || (count && !views) || count>2 || !backend || !backend->enable || !backend->corona || !backend->reflection)return RF_RANGE;
    for(view=0;view<count;++view)if(views[view]==current)break;
    if(view==count)return RF_OK;
    node=list->sentinel.next;
    for(i=0;i<list->count;++i) {
        if(!node || node==&list->sentinel || !node->next || !node->previous || node->next->previous!=node || node->previous->next!=node)return RF_RANGE;
        node=node->next;
    }
    if(node!=&list->sentinel || !list->sentinel.previous || list->sentinel.previous->next!=&list->sentinel)return RF_RANGE;
    status=backend->enable(backend->context,1);if(status)return status;
    for(node=list->sentinel.next;node!=&list->sentinel;node=node->next) {
        rf_glare_base_owner *owner=(rf_glare_base_owner *)((unsigned char *)node-offsetof(rf_glare_state,link));
        if(owner->flags&1)continue;
        if(owner->state.flags&0x80000000u) {
            status=backend->corona(backend->context,owner,view);if(status)break;
        } else {owner->state.samples[view]=0;owner->state.samples[2+view]=0;}
        if((uint8_t)reflections) {status=backend->reflection(backend->context,owner);if(status)break;}
        owner->state.flags&=0x7fffffffu;
    }
    disabled=backend->enable(backend->context,0);return status?status:disabled;
}
int32_t rf_glare_name_lookup(const char *const *names,uint32_t count,const char *name)
{
    uint32_t i;if(!name || (count && !names) || count>INT_MAX)return -1;
    for(i=0;i<count;++i)if(!strcmp(names[i]?names[i]:"",name))return (int32_t)i;
    return -1;
}
int32_t rf_emitter_name_lookup(const char *const *names,uint32_t count,const char *name)
{
    uint32_t i;if(!name || (count && !names) || count>INT_MAX)return -1;
    for(i=0;i<count;++i)if(clutter_skin_name_equal(names[i]?names[i]:"",name))return (int32_t)i;
    return -1;
}
static int clutter_class_measure(const rf_clutter_definition *d,const rf_clutter_class_binding *b,
    uint32_t *strings,uint32_t *emitters)
{
    const char *names[3];uint32_t j,n=0;
    if(!d || !b || !*d->name || d->emitter_count>16 || (d->emitter_count && !b->emitters) ||
       b->material>255 || (d->flags&~511u) || (d->model_kind!=1 && d->model_kind!=3) ||
       !isfinite(d->life) || !isfinite(d->radius) || !isfinite(d->emitter_lifetime))return RF_RANGE;
    names[0]=d->name;names[1]=d->model;names[2]=d->corpse;
    for(j=0;j<3;++j){const char *end=memchr(names[j],0,64);if(!end)return RF_RANGE;n+=(uint32_t)(end-names[j])+1;}
    *strings=n;*emitters=d->emitter_count*4;return RF_OK;
}
int rf_clutter_classes_open_source(rf_clutter_class_fetch fetch,void *context,
    uint32_t count,uint32_t budget,rf_clutter_classes *owner)
{
    rf_clutter_classes v={0};uint64_t bytes=sizeof(v),emitter_bytes=0,string_bytes=0;
    uint32_t i,j,ns,ne;int32_t *emitter;char *text;int status;
    const rf_clutter_definition *d;const rf_clutter_class_binding *b;
    if(!owner || owner->storage || owner->items || owner->count || owner->allocated_bytes ||
       !fetch || count>INT_MAX)return RF_RANGE;
    for(i=0;i<count;++i) {
        d=NULL;b=NULL;status=fetch(context,i,&d,&b);if(status)return status;
        status=clutter_class_measure(d,b,&ns,&ne);if(status)return status;
        string_bytes+=ns;emitter_bytes+=ne;
    }
    bytes+=(uint64_t)count*sizeof(*v.items)+emitter_bytes+string_bytes;
    if(bytes>budget || bytes>UINT32_MAX)return RF_RANGE;
    v.allocated_bytes=(uint32_t)bytes;v.count=count;
    if(count) {
        v.storage=malloc((size_t)(bytes-sizeof(v)));if(!v.storage)return RF_IO;
        v.items=v.storage;memset(v.items,0,(size_t)count*sizeof(*v.items));
        emitter=(int32_t *)(v.items+count);text=(char *)emitter+(size_t)emitter_bytes;
        for(i=0;i<count;++i) {
            rf_clutter_class *c=v.items+i;const char *names[3];
            const char **outputs[]={&c->name,&c->model,&c->corpse};
            d=NULL;b=NULL;status=fetch(context,i,&d,&b);if(status)goto failed;
            status=clutter_class_measure(d,b,&ns,&ne);if(status)goto failed;
            if(ns>string_bytes || ne>emitter_bytes){status=RF_RANGE;goto failed;}
            string_bytes-=ns;emitter_bytes-=ne;names[0]=d->name;names[1]=d->model;names[2]=d->corpse;
            for(j=0;j<3;++j){size_t n=strlen(names[j])+1;*outputs[j]=text;memcpy(text,names[j],n);text+=n;}
            c->emitter_count=d->emitter_count;
            if(c->emitter_count){c->emitters=emitter;memcpy(emitter,b->emitters,c->emitter_count*4);emitter+=c->emitter_count;}
            c->emitter_lifetime=d->emitter_lifetime;c->model_kind=d->model_kind;c->life=d->life;c->radius=d->radius;
            c->material=b->material;c->flags=d->flags;c->sound=b->sound;c->explosion=b->explosion;c->glare=b->glare;c->rod=b->rod;
            c->light_tag=-1;c->screen_width=d->screen_width;c->screen_height=d->screen_height;
        }
    }
    if(string_bytes || emitter_bytes){status=RF_FORMAT;goto failed;}
    *owner=v;return RF_OK;
failed:
    free(v.storage);return status;
}
typedef struct clutter_array_source {const rf_clutter_definition *definitions;const rf_clutter_class_binding *bindings;} clutter_array_source;
static int clutter_array_fetch(void *context,uint32_t index,const rf_clutter_definition **definition,const rf_clutter_class_binding **binding)
{const clutter_array_source *source=context;*definition=source->definitions+index;*binding=source->bindings+index;return RF_OK;}
int rf_clutter_classes_open(const rf_clutter_definition *definitions,const rf_clutter_class_binding *bindings,
    uint32_t count,uint32_t budget,rf_clutter_classes *owner)
{
    clutter_array_source source={definitions,bindings};if(count && (!definitions || !bindings))return RF_RANGE;
    return rf_clutter_classes_open_source(clutter_array_fetch,&source,count,budget,owner);
}
void rf_clutter_classes_close(rf_clutter_classes *owner)
{if(owner){free(owner->storage);memset(owner,0,sizeof(*owner));}}
int rf_clutter_create_glares(rf_clutter_class *c,rf_clutter_state *s,const rf_clutter_create_backend *backend)
{
    rf_clutter_create_request q={0};int status;int32_t value;uint32_t i;char tag_name[32];
    if(!c || !s || !backend || !backend->call || c->corona_count>4)return RF_RANGE;
#define CLUTTER_CALL(op) do {status=backend->call(backend->context,s,(op),&q,&value);if(status)return status;} while(0)
    if(c->glare!=-1 && !(c->flags&0x400)) {
        for(i=1;;++i) {
            if(i>INT_MAX)return RF_RANGE;
            snprintf(tag_name,sizeof(tag_name),"corona_%u",i);
            q=(rf_clutter_create_request){{s->model},tag_name,NULL};CLUTTER_CALL(RF_CLUTTER_TAG);
            if(value<0)break;
            if(c->corona_count<4)c->coronas[c->corona_count++]=value;
        }
        c->flags|=0x400;
    }
    for(i=0;i<c->corona_count;++i) {
        q=(rf_clutter_create_request){{s->handle,(uint32_t)c->coronas[i],(uint32_t)c->glare,0},NULL,NULL};
        CLUTTER_CALL(RF_CLUTTER_GLARE);
    }
#undef CLUTTER_CALL
    return RF_OK;
}
int rf_clutter_create(rf_clutter_class *classes,uint32_t count,int32_t index,
    int32_t shield_class,const char *name,int32_t identifier,const float position[3],
    const float matrix[9],uint32_t persistent,int32_t now_ms,int32_t *next_slot,
    rf_object_list *list,const rf_clutter_create_backend *backend,rf_clutter_state **out)
{
    rf_clutter_class *c;rf_clutter_state *s=NULL;rf_clutter_create_descriptor d={0};
    rf_clutter_create_request q={0};int status;int32_t value,tag,rod,deadline=-1;uint32_t i;
    double milliseconds;
    if(!classes || count>INT_MAX || index<0 || (uint32_t)index>=count || !name ||
       !position || !matrix || !next_slot || *next_slot<0 || !list || !out ||
       !backend || !backend->allocate || !backend->call || now_ms<0 || now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    c=classes+index;
    if(!c->name || !c->model || !c->corpse || (c->emitter_count && !c->emitters) ||
       c->emitter_count>INT_MAX || c->corona_count>4 || c->material>255 ||
       !isfinite(c->life) || !isfinite(c->radius) || !isfinite(c->emitter_lifetime))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(position[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(matrix[i]))return RF_RANGE;
    for(i=0;i<count;++i)if(!classes[i].name)return RF_RANGE;
    if(c->emitter_lifetime>0) {
        milliseconds=(double)c->emitter_lifetime*1000.0+0.5;
        if(milliseconds>RF_TIMER_PERIOD)return RF_RANGE;
        status=rf_timer_set(&deadline,now_ms,(int32_t)milliseconds);if(status)return status;
    }
    d.model=c->model;d.kind=c->model_kind;d.material=c->material;d.flags=(c->flags&6)?0x20:0;
    d.allocation_flags=index==shield_class?0x100000:0;d.identifier=identifier;
    memcpy(d.position,position,sizeof(d.position));memcpy(d.matrix,matrix,sizeof(d.matrix));d.radius=c->radius;
    status=backend->allocate(backend->context,&d,&s);if(status)return status;
    *out=s;if(!s)return RF_OK;
    if(c->flags&0x20)s->flags|=0x100000;
    if(c->flags&2)s->flags|=0x40000;
    s->definition=c;s->class_index=index;s->name=*name?name:c->name;s->byte_cc=0;
    s->health=c->life;if(c->life<0){s->health=100;s->flags|=4;}s->armor=0;
#define CLUTTER_CALL(op) do {status=backend->call(backend->context,s,(op),&q,&value);if(status)return status;} while(0)
    s->sound=-1;
    if(c->sound>=0) {
        q=(rf_clutter_create_request){{(uint32_t)c->sound,0x3f800000,0},NULL,s->position};
        CLUTTER_CALL(RF_CLUTTER_SOUND);q=(rf_clutter_create_request){{(uint32_t)value},NULL,NULL};
        CLUTTER_CALL(RF_CLUTTER_SOUND_HANDLE);s->sound=value;
    }
    if(c->flags&1)s->flags|=0x1000;
    s->corpse=-1;
    if(*c->corpse)for(i=0;i<count;++i)if(clutter_skin_name_equal(c->corpse,classes[i].name)){s->corpse=(int32_t)i;break;}
    s->timer_a4=-1;s->word_a8=-1;
    for(i=0;i<c->emitter_count;++i)if(c->emitters[i]>=0) {
        q=(rf_clutter_create_request){{s->handle,(uint32_t)c->emitters[i],s->first_word,1},NULL,s->position};
        CLUTTER_CALL(RF_CLUTTER_EMITTER);
        if(value) {
            q=(rf_clutter_create_request){{(uint32_t)value,s->emitter_head},NULL,NULL};
            s->emitter_head=(uint32_t)value;CLUTTER_CALL(RF_CLUTTER_EMITTER_PREPEND);
        }
    }
    s->timer_b0=deadline;c->timer=now_ms;s->timer_b4=-1;s->word_b8=0;s->skin=-1;s->sound_d0=-1;
    status=rf_clutter_create_glares(c,s,backend);if(status)return status;
    q=(rf_clutter_create_request){{s->model},"corona_rod1",NULL};CLUTTER_CALL(RF_CLUTTER_TAG);tag=value;
    if(tag>=0) {
        q.text="corona_rod2";CLUTTER_CALL(RF_CLUTTER_TAG);rod=value;if(rod<0)return RF_FORMAT;
        if(c->rod>=0) {
            q=(rf_clutter_create_request){{s->handle,(uint32_t)c->rod,(uint32_t)tag,(uint32_t)rod,UINT32_MAX},NULL,NULL};
            CLUTTER_CALL(RF_CLUTTER_ROD);
        }
    }
    if((c->flags&0x10) && !(c->flags&0x800)) {
        q=(rf_clutter_create_request){{s->model},"light_prop",NULL};CLUTTER_CALL(RF_CLUTTER_TAG);
        c->light_tag=value;c->flags|=0x800;
    }
    if(c->flags&8) {
        q=(rf_clutter_create_request){{s->handle,UINT32_MAX,c->screen_width,c->screen_height,1},NULL,NULL};
        CLUTTER_CALL(RF_CLUTTER_SCREEN);
    }
    if(c->explosion!=-1) {
        q=(rf_clutter_create_request){{(uint32_t)c->explosion},NULL,NULL};CLUTTER_CALL(RF_CLUTTER_EXPLOSION);
    }
    if((s->physics_flags&0x20) && !(s->flags&0x8000)) {
        q=(rf_clutter_create_request){{s->token},NULL,NULL};CLUTTER_CALL(RF_CLUTTER_COLLISION);
    }
    rf_object_list_append(list,&s->link);s->slot=UINT16_MAX;
    if(persistent && *next_slot<3200) {
        s->slot=(uint16_t)*next_slot;
        q=(rf_clutter_create_request){{(uint32_t)(*next_slot)++,1},NULL,NULL};CLUTTER_CALL(RF_CLUTTER_SLOT);
    }
#undef CLUTTER_CALL
    return RF_OK;
}
int rf_clutter_skin_apply(const rf_clutter_skin_variant *variants,uint32_t count,
    const char *name,uint32_t parent,uint32_t model,rf_clutter_skin_glare *glares,uint32_t glare_count,
    const void *const *glare_classes,uint32_t class_count,
    const rf_clutter_skin_backend *backend,int32_t *selected)
{
    const rf_clutter_skin_variant *variant;rf_model_material_record *materials=NULL;
    uint32_t index,i;int32_t material_count=0,texture;int status;
    if(!name || !selected || (count && !variants) || count>INT_MAX ||
        (glare_count && !glares) || (class_count && !glare_classes) ||
        !backend || !backend->materials || !backend->texture)return RF_RANGE;
    for(index=0;index<count;++index) {
        if(!variants[index].name)return RF_RANGE;
        if(clutter_skin_name_equal(variants[index].name,name))break;
    }
    if(index==count){*selected=-1;return RF_OK;}
    variant=variants+index;
    if(variant->texture_count>0) {
        if(!variant->textures)return RF_RANGE;
        for(i=0;i<(uint32_t)variant->texture_count;++i)if(!variant->textures[i])return RF_RANGE;
    }
    if(variant->glare_class>=0 && (uint32_t)variant->glare_class<class_count)
        for(i=0;i<glare_count;++i)if(glares[i].parent==parent) {
            glares[i].class_index=variant->glare_class;
            glares[i].class_record=glare_classes[variant->glare_class];
        }
    status=backend->materials(backend->context,model,&materials,&material_count);if(status)return status;
    if(material_count>0 && variant->texture_count>0 && !materials)return RF_RANGE;
    for(i=0;(int32_t)i<variant->texture_count && (int32_t)i<material_count;++i) {
        status=backend->texture(backend->context,variant->textures[i],-1,1,&texture);if(status)return status;
        memcpy(materials[i].bytes+0x10,&texture,4);
    }
    *selected=(int32_t)index;return RF_OK;
}
int rf_model_skeletal_register(rf_model_skeletal_registration *node,
    rf_model_skeletal_registration **head,uint32_t limit)
{
    rf_model_skeletal_registration *p,*tail;uint32_t count=1;
    if(!node || !head || !limit || !node->loaded || !node->active || node->next || node->previous)return RF_RANGE;
    if(*head) {
        p=*head;do {
            if(count++>=limit || p==node || !p->next || !p->previous || p->next->previous!=p || p->previous->next!=p)return RF_RANGE;
            p=p->next;
        } while(p!=*head);
        tail=(*head)->previous;node->previous=tail;node->next=*head;(*head)->previous=node;tail->next=node;
    } else node->next=node->previous=node;
    *head=node;return RF_OK;
}

int rf_model_skeletal_retire(rf_model_skeletal_registration *node,
    rf_model_skeletal_registration **head,uint32_t limit,rf_motion_playback_resource *resources,uint32_t resource_count)
{
    rf_model_skeletal_registration *p;rf_motion_slot_state *active;uint32_t visits=0,found=0,i;int status;
    if(!node)return RF_RANGE;if(!node->loaded)return RF_OK;
    if(!head || !*head || !limit || !node->active)return RF_RANGE;
    p=*head;do {
        if(++visits>limit || !p->next || !p->previous || p->next->previous!=p || p->previous->next!=p)return RF_RANGE;
        if(p==node)found=1;p=p->next;
    } while(p!=*head);
    if(!found)return RF_NOT_FOUND;
    active=node->active;if(active->count>16 || (active->count && !resources))return RF_RANGE;
    for(i=0;i<active->count;++i) {
        int32_t id=active->slots[i].motion;
        if(id<0 || (uint32_t)id>=resource_count || resources[id].references<0)return RF_RANGE;
    }
    active->freeze_slot=active->primary_slot=active->dominant_slot=-1;
    while(active->count) {
        int32_t id=active->slots[0].motion;status=rf_motion_remove_slot(active,id,&resources[id].references);if(status)return status;
    }
    if(*head==node)*head=node->next==node?NULL:node->next;
    if(node->next!=node){node->previous->next=node->next;node->next->previous=node->previous;}
    node->next=node->previous=NULL;return RF_OK;
}

int rf_model_release(rf_model_release_state *state,const rf_model_release_backend *backend)
{
    if(!state || !backend || !backend->payload || !backend->materials || !backend->recycle)return RF_RANGE;
    if((state->kind==2 || state->kind==3) && state->payload) {
        backend->payload(backend->context,state->kind,state->payload);state->payload=0;
    }
    if(state->materials)backend->materials(backend->context,state->materials);
    backend->recycle(backend->context,state);return RF_OK;
}

int rf_model_register_motion(rf_model_motion_registry *registry,uint32_t identity,
    uint8_t flag,int32_t *index,int *added)
{
    uint32_t i;
    if(!registry || !registry->identities || !registry->flags || !identity || !index || !added ||
       registry->count>registry->capacity || registry->capacity>INT32_MAX)return RF_RANGE;
    for(i=0;i<registry->count;++i)if(registry->identities[i]==identity && registry->flags[i]==flag) {
        *index=(int32_t)i;*added=0;return RF_OK;
    }
    if(registry->count==registry->capacity)return RF_RANGE;
    i=registry->count;registry->identities[i]=identity;registry->flags[i]=flag;registry->count=i+1;
    *index=(int32_t)i;*added=1;return RF_OK;
}

int rf_model_local_view(const rf_model_projection *world,const float position[3],
    const float orientation[9],rf_model_projection *local)
{
    static const uint8_t order[9][3]={{0,2,1},{2,1,0},{1,0,2},{2,1,0},{2,1,0},{0,1,2},{2,1,0},{2,0,1},{1,0,2}};
    rf_model_projection result;float delta[3];unsigned i,j,k;
    if(!world || !position || !orientation || !local)return RF_RANGE;
    result=*world;
    for(i=0;i<3;++i)delta[i]=world->camera[i]-position[i];
    for(i=0;i<3;++i) {
        result.camera[i]=(float)(((double)delta[2]*orientation[i*3+2]+(double)delta[1]*orientation[i*3+1])+(double)delta[0]*orientation[i*3]);
        for(j=0;j<3;++j) {
            double terms[3];const uint8_t *o=order[i*3+j];
            for(k=0;k<3;++k)terms[k]=(double)world->rotation[i*3+k]*orientation[j*3+k];
            result.rotation[i*3+j]=(float)((terms[o[0]]+terms[o[1]])+terms[o[2]]);
        }
    }
    *local=result;return RF_OK;
}

int rf_model_emit_clip_polygon(uint8_t *const *records,uint32_t count,uint8_t common,
    const uint16_t triangle[3],uint16_t base,const rf_model_clip_projection *projection,
    const rf_model_render_output *attributes,float depth_factor,rf_model_triangle_output *output)
{
    uint16_t indices[48];uint32_t i,needed;
    if(count<3 || common)return RF_OK;
    if(!records || !triangle || !projection || !attributes || !output || !output->vertices || !output->indices || count>48)return RF_RANGE;
    needed=(count-2)*3;
    if(output->vertex_count>=output->vertex_capacity || count>=output->vertex_capacity-output->vertex_count ||
        output->index_count>=output->index_capacity || needed>=output->index_capacity-output->index_count ||
        output->vertex_count>65535 || count>65536-output->vertex_count)return RF_RANGE;
    for(i=0;i<count;++i)if(!records[i] || (!(records[i][25]&4) && records[i][26]>2))return RF_RANGE;
    for(i=0;i<count;++i) {
        uint8_t *record=records[i];
        if(record[25]&4) {
            uint8_t *vertex=output->vertices[output->vertex_count];float z,reciprocal,value,depth,biased;uint32_t bits;
            indices[i]=(uint16_t)output->vertex_count++;
            rf_model_project_clip_vertex(projection,record);
            memcpy(vertex,record+12,8);memcpy(&z,record+8,4);
            reciprocal=(float)(1.0/z);memcpy(record+20,&reciprocal,4);
            value=reciprocal*attributes->reciprocal_scale;memcpy(vertex+12,&value,4);
            value=reciprocal*attributes->depth_scale;memcpy(vertex+8,&value,4);
            vertex[16]=record[46];vertex[17]=record[45];vertex[18]=record[44];vertex[19]=attributes->alpha;
            memcpy(vertex+24,record+28,8);
            depth=(float)(255.0-(double)depth_factor*z);
            if(!(depth>=0))depth=0;else if(depth>255)depth=255;
            biased=depth+12582912.0f;memcpy(&bits,&biased,4);vertex[23]=(uint8_t)bits;
        } else indices[i]=triangle[record[26]];
    }
    for(i=1;i+1<count;++i) {
        output->indices[output->index_count++]=(uint16_t)(base+indices[0]);
        output->indices[output->index_count++]=(uint16_t)(base+indices[i]);
        output->indices[output->index_count++]=(uint16_t)(base+indices[i+1]);
    }
    return RF_OK;
}

int rf_model_project_clip_vertex(const rf_model_clip_projection *view,uint8_t record[48])
{
    float position[3],projected[3],reciprocal,x;double y;
    if(!view || !record)return RF_RANGE;
    if(record[25]&3)return RF_OK;
    memcpy(position,record,12);
    if(view->clamp && !(position[2]>0)) {record[25]|=2;return RF_OK;}
    record[25]|=1;
    reciprocal=position[2]==0 || isnan(position[2])?FLT_MAX:(float)(1.0/position[2]);
    projected[2]=reciprocal;
    if(view->depth_bias!=0 && !isnan(view->depth_bias) &&
        ((double)view->depth_bias*20<position[2] || isnan(position[2])))
        projected[2]=(float)(1.0/((double)position[2]-view->depth_bias));
    x=(float)((double)reciprocal*position[0]+1);
    y=1-(double)reciprocal*position[1];
    if(view->clamp) {
        if(!(x>0))x=0;else if(x>=2)x=2;
        if(!(y>0))y=0;else if(y>=2)y=2;
    }
    projected[0]=(float)((double)view->scale[0]*x+view->offset[0]);
    projected[1]=(float)((double)view->scale[1]*y+view->offset[1]);
    memcpy(record+12,projected,12);return RF_OK;
}

static int model_clip_edge(rf_model_clip_pool *pool,uint32_t plane,const uint8_t *a,const uint8_t *b,
    const rf_model_clip_planes *planes,const rf_model_projection *view,uint32_t mode,uint32_t attributes,uint8_t **out)
{
    float inside[3],outside[3],position[3];double factor;uint32_t slot;int status;
    status=rf_model_clip_pool_allocate(pool,&slot);if(status)return status;
    memcpy(inside,a,12);memcpy(outside,b,12);
    status=rf_model_clip_intersection(plane,inside,outside,planes,position,&factor);if(status)return status;
    memcpy(pool->records[slot],position,12);
    status=rf_model_clip_attributes(a,b,factor,attributes,pool->records[slot]);if(status)return status;
    status=rf_model_classify_clip_vertex(mode,view,pool->records[slot]);if(status)return status;
    *out=pool->records[slot];return RF_OK;
}

int rf_model_clip_polygon(rf_model_clip_pool *pool,uint8_t *const *original,uint32_t count,
    const rf_model_clip_planes *planes,const rf_model_projection *view,uint32_t mode,uint32_t attributes,
    uint8_t *result[48],uint32_t *result_count,uint8_t mask[2])
{
    uint8_t *lists[2][50];uint32_t active=0,plane,i;int status;
    if(!pool || !original || !planes || !view || !result || !result_count || !mask || count<2 || count>46 || attributes>7)return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!original[i] || (original[i][25]&4))return RF_RANGE;
        lists[0][i]=original[i];
    }
    for(plane=1;plane<=64;plane<<=1)if(mask[0]&plane) {
        uint32_t next=active^1,produced=0;
        lists[active][count]=lists[active][0];lists[active][count+1]=lists[active][1];mask[0]=0;mask[1]=255;
        for(i=1;i<=count;++i) {
            uint8_t *current=lists[active][i];
            if(!(current[24]&plane)) {
                if(produced==48)return RF_RANGE;
                lists[next][produced++]=current;mask[0]|=current[24];mask[1]&=current[24];
            } else {
                unsigned side;
                for(side=0;side<2;++side) {
                    uint8_t *neighbor=lists[active][side?i+1:i-1],*created;
                    if(neighbor[24]&plane)continue;
                    if(produced==48)return RF_RANGE;
                    status=model_clip_edge(pool,plane,neighbor,current,planes,view,mode,attributes,&created);
                    if(status) {mask[1]=255;*result_count=0;return status;}
                    lists[next][produced++]=created;mask[0]|=created[24];mask[1]&=created[24];
                }
                if(current[25]&4) {
                    uint32_t slot;for(slot=0;slot<48;++slot)if(current==pool->records[slot])break;
                    status=rf_model_clip_pool_release(pool,slot);if(status)return status;
                }
            }
        }
        count=produced;active=next;if(mask[1])break;
        if(count>48)return RF_RANGE;
    }
    memcpy(result,lists[active],count*sizeof(*result));*result_count=count;return RF_OK;
}

void rf_model_clip_pool_reset(rf_model_clip_pool *pool)
{
    uint32_t i;if(!pool)return;
    for(i=0;i<48;++i)pool->order[i]=i;
    pool->used=0;pool->live=0;
}
int rf_model_clip_pool_allocate(rf_model_clip_pool *pool,uint32_t *slot)
{
    uint32_t index;
    if(!pool || !slot || pool->used>=48)return RF_RANGE;
    index=pool->order[pool->used];
    if(index>=48 || (pool->live&((uint64_t)1<<index)))return RF_RANGE;
    if(++pool->used>=48)return RF_RANGE;
    pool->records[index][25]=4;pool->live|=(uint64_t)1<<index;*slot=index;return RF_OK;
}
int rf_model_clip_pool_release(rf_model_clip_pool *pool,uint32_t slot)
{
    if(!pool || !pool->used || pool->used>=48 || slot>=48 || !(pool->live&((uint64_t)1<<slot)))return RF_RANGE;
    pool->order[--pool->used]=slot;pool->live&=~((uint64_t)1<<slot);return RF_OK;
}

static uint8_t model_clip_mask(const float position[3],const rf_model_projection *view)
{
    uint8_t clip=0;
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
    return clip;
}

int rf_model_classify_clip_vertex(uint32_t mode,const rf_model_projection *view,uint8_t record[48])
{
    float position[3];
    if(!view || !record)return RF_RANGE;
    if(mode==0x66) {memcpy(position,record,12);record[24]=model_clip_mask(position,view);}
    return RF_OK;
}

int rf_model_clip_intersection(uint32_t plane,const float inside[3],const float outside[3],
    const rf_model_clip_planes *planes,float position[3],double *factor)
{
    double t,denominator,numerator;float result[3];unsigned i;
    if(!inside || !outside || !planes || !position || !factor || !plane || plane>64 || (plane&(plane-1)))return RF_RANGE;
    if(plane&3) {
        float depth=plane==2?planes->far_depth:planes->near_depth;
        denominator=(double)outside[2]-inside[2];
        t=denominator==0 || isnan(denominator)?1:((double)depth-inside[2])/denominator;
        result[2]=depth;
        for(i=0;i<2;++i)result[i]=(float)(((double)outside[i]-inside[i])*t+inside[i]);
    } else if(plane==64) {
        float delta[3],offset[3],denom,tf;
        for(i=0;i<3;++i) {delta[i]=outside[i]-inside[i];offset[i]=inside[i]-planes->point[i];}
        denominator=-(((double)planes->normal[0]*delta[0]+(double)planes->normal[1]*delta[1])+(double)planes->normal[2]*delta[2]);
        denom=(float)denominator;
        numerator=((double)planes->normal[0]*offset[0]+(double)planes->normal[1]*offset[1])+(double)planes->normal[2]*offset[2];
        tf=denominator==0 || isnan(denominator)?1:(float)(numerator/denom);t=tf;
        for(i=0;i<3;++i) {float scaled=delta[i]*tf;result[i]=inside[i]+scaled;}
    } else {
        unsigned axis=(plane&12)?0:1;double a=inside[axis],b=outside[axis];
        if(plane&20) {a=-a;b=-b;}
        numerator=a-inside[2];t=numerator/((numerator-b)+outside[2]);
        for(i=0;i<2;++i)result[i]=(float)(((double)outside[i]-inside[i])*t+inside[i]);
        result[2]=result[(plane&48)?1:0];if(plane&20)result[2]=-result[2];
    }
    memcpy(position,result,12);*factor=t;return RF_OK;
}

int rf_model_clip_attributes(const uint8_t inside[48],const uint8_t outside[48],double factor,
    uint32_t flags,uint8_t result[48])
{
    uint8_t value[48];unsigned i;
    if(!inside || !outside || !result || !(factor>=0 && factor<=1) || (flags&~7u))return RF_RANGE;
    memcpy(value,result,sizeof(value));
    for(i=0;i<4;++i)if(flags&(i<2?1u:2u)) {
        float a,b,out;memcpy(&a,inside+28+i*4,4);memcpy(&b,outside+28+i*4,4);
        out=(float)(((double)b-a)*factor+a);memcpy(value+28+i*4,&out,4);
    }
    if(flags&4)for(i=44;i<47;++i)value[i]=(uint8_t)(int32_t)(((double)outside[i]-inside[i])*factor+inside[i]);
    memcpy(result,value,sizeof(value));return RF_OK;
}

int rf_model_route_triangle(const rf_model_render_cache *cache,uint32_t count,const uint16_t indices[3],
    uint16_t flags,const rf_model_projection *view,uint32_t *route)
{
    const rf_model_render_cache *a,*b,*c;uint32_t facing;uint8_t any,common;int status;unsigned i;
    if(!cache || !indices || !view || !route)return RF_RANGE;
    for(i=0;i<3;++i)if(indices[i]>=count || indices[i]>INT16_MAX)return RF_RANGE;
    a=cache+indices[0];b=cache+indices[1];c=cache+indices[2];
    any=a->clip|b->clip|c->clip;common=a->clip&b->clip&c->clip;
    if((view->screen_clip && any) || (!view->screen_clip && view->compute_clip && common)) {
        *route=RF_MODEL_TRIANGLE_REJECT;return RF_OK;
    }
    status=rf_model_triangle_facing(a->world,b->world,c->world,flags,view->perspective,view->camera,view->rotation+6,&facing);
    if(status)return status;
    *route=!facing?RF_MODEL_TRIANGLE_REJECT:view->compute_clip && any?RF_MODEL_TRIANGLE_CLIP:RF_MODEL_TRIANGLE_DIRECT;
    return RF_OK;
}

int rf_model_route_static_triangle(const rf_model_render_cache *cache,uint32_t count,const uint16_t indices[3],
    uint16_t flags,const float plane[4],const rf_model_projection *view,uint32_t *route)
{
    uint8_t any,common;uint32_t i,facing=1;double dot;
    if(!cache || !indices || !plane || !view || !route)return RF_RANGE;
    for(i=0;i<3;++i)if(indices[i]>=count || indices[i]>INT16_MAX)return RF_RANGE;
    any=cache[indices[0]].clip|cache[indices[1]].clip|cache[indices[2]].clip;
    common=cache[indices[0]].clip&cache[indices[1]].clip&cache[indices[2]].clip;
    if((view->screen_clip && any) || (!view->screen_clip && view->compute_clip && common)) {
        *route=RF_MODEL_TRIANGLE_REJECT;return RF_OK;
    }
    if(!(flags&0x20)) {
        const float *direction=view->perspective?view->camera:view->rotation+6;
        dot=((double)plane[0]*direction[0]+(double)plane[1]*direction[1])+(double)plane[2]*direction[2];
        if(view->perspective)dot+=plane[3];
        facing=view->perspective?(dot>0):!(dot>0);
    }
    *route=!facing?RF_MODEL_TRIANGLE_REJECT:view->compute_clip && any?RF_MODEL_TRIANGLE_CLIP:RF_MODEL_TRIANGLE_DIRECT;
    return RF_OK;
}

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
        clip=model_clip_mask(position,view);
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

/* Original static path 52e1b5..52e341: no skinning, different sum order,
 * the Z subtraction stays extended during rotation, and the vertical screen
 * calculation consumes the stored reciprocal. */
int rf_model_project_static_vertex(const float world[3],const rf_model_projection *view,
    rf_model_render_cache *cache,float clip_position[3],uint8_t vertex[40],uint32_t *visible)
{
    float delta[3],position[3],depth,biased;double reciprocal,last_delta;uint32_t bits;uint8_t clip=0;unsigned i;
    const float *m;
    if(!world || !view || !cache || !clip_position || !vertex || !visible)return RF_RANGE;
    m=view->rotation;
    for(i=0;i<3;++i)delta[i]=world[i]-view->camera[i];
    last_delta=(double)world[2]-view->camera[2];
    position[0]=(float)(((double)m[0]*delta[0]+(double)m[2]*last_delta)+(double)m[1]*delta[1]);
    position[1]=(float)(((double)m[3]*delta[0]+(double)m[5]*last_delta)+(double)m[4]*delta[1]);
    position[2]=(float)(((double)m[6]*delta[0]+(double)m[8]*last_delta)+(double)m[7]*delta[1]);
    if(!view->perspective)position[2]=view->fixed_depth;
    if(view->compute_clip) {
        clip=model_clip_mask(position,view);
        memcpy(clip_position,position,sizeof(position));
    }
    depth=(float)(255.0-(double)view->depth_factor*position[2]);
    if(!(depth>=0))depth=0;else if(depth>255)depth=255;
    biased=depth+12582912.0f;memcpy(&bits,&biased,4);cache->depth=(uint8_t)bits;vertex[23]=cache->depth;
    reciprocal=1.0/(double)position[2];
    cache->projected[2]=(float)reciprocal;
    cache->projected[0]=(float)((reciprocal*position[0])*view->screen[0]+view->screen[2]);
    cache->projected[1]=(float)(((double)cache->projected[2]*view->screen[1])*position[1]+view->screen[3]);
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

/* Static52e11b..52e43e: unlike the skeletal loop, no normalization or
 * world/second-cache publication occurs. Duplicate outputs use the referenced
 * cache entry without copying its projected/depth/RGB fields to this entry. */
int rf_model_geometry_render_static_batch(const rf_model_geometry *geometry,uint32_t batch,
    const rf_model_projection *view,const rf_model_lighting *lights,
    const rf_model_render_output *output,const uint8_t (*colors)[3],rf_model_render_buffers *buffers)
{
    const rf_model_draw_batch *draw;uint32_t i;int status;
    if(!geometry || !geometry->batches || batch>=geometry->batch_count || !view || !lights || !output ||
       !buffers || !buffers->cache || !buffers->clip || !buffers->vertices)return RF_RANGE;
    draw=geometry->batches+batch;
    if(draw->vertices>buffers->capacity || draw->first_vertex>geometry->vertex_count ||
       draw->vertices>geometry->vertex_count-draw->first_vertex || !geometry->vertices || !geometry->reuse)return RF_RANGE;
    for(i=0;i<draw->vertices;++i)if(geometry->reuse[draw->first_vertex+i]>0 &&
        (uint32_t)geometry->reuse[draw->first_vertex+i]>i)return RF_RANGE;
    for(i=0;i<draw->vertices;++i) {
        const rf_model_vertex *v=geometry->vertices+draw->first_vertex+i;
        int32_t distance=geometry->reuse[draw->first_vertex+i];uint32_t visible=0;
        rf_model_render_cache *cache=buffers->cache+i,*source=cache;
        rf_model_render_output attributes=*output;
        if(distance>0) {
            source=cache-distance;cache->clip=source->clip;visible=!source->clip;
        } else {
            status=rf_model_project_static_vertex(v->position,view,cache,buffers->clip[i],buffers->vertices[i],&visible);
            if(status)return status;
            if(visible && output->lighting && !colors) {
                status=rf_model_vertex_lighting(v->normal,lights->lights,lights->ambient,cache->rgb);
                if(status)return status;
            }
        }
        if(visible) {
            attributes.lighting=colors?0:1;
            if(colors)memcpy(attributes.rgb,colors[i],3);
            emit_render_vertex(source,&attributes,v->uv,buffers->vertices[i]);
        }
    }
    return RF_OK;
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

int rf_model_find_bone_substring(const rf_model_name *bones,uint32_t count,
    rf_model_name query,int32_t *index)
{
    uint32_t n;size_t start,i;
    if(!index || (count && !bones) || count>(uint32_t)INT32_MAX)return RF_RANGE;
    if(!valid_name(query))return RF_FORMAT;
    for(n=0;n<count;++n) {
        rf_model_name name=bones[n];
        if(!valid_name(name))return RF_FORMAT;
        if(query.length>name.length)continue;
        for(start=0;start<=name.length-query.length;++start) {
            for(i=0;i<query.length;++i)if(name.data[start+i]!=query.data[i])break;
            if(i==query.length){*index=(int32_t)n;return RF_OK;}
        }
    }
    return RF_NOT_FOUND;
}
static unsigned char fold(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

int rf_model_find_static_tag(const rf_model_name *names,uint32_t count,
    rf_model_name query,int32_t *index)
{
    uint32_t n;size_t i;
    if(!index || (count && !names) || count>(uint32_t)INT32_MAX)return RF_RANGE;
    if(!query.data && !query.length)return RF_NOT_FOUND;
    if(!valid_name(query))return RF_FORMAT;
    for(n=0;n<count;++n) {
        if(!valid_name(names[n]))return RF_FORMAT;
        if(query.length>names[n].length)continue;
        for(i=0;i<query.length;++i)
            if(fold((unsigned char)names[n].data[i])!=fold((unsigned char)query.data[i]))break;
        if(i==query.length){*index=(int32_t)n;return RF_OK;}
    }
    return RF_NOT_FOUND;
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

int rf_model_death_bones(const rf_model_bone *bones,uint32_t count,int32_t out[3])
{
    int32_t first=-1,second=-1,head=-1,parent;uint32_t i;
    if(!out || count>50 || (count && !bones))return RF_RANGE;
    for(i=0;i<count;++i)if(!memchr(bones[i].name,0,sizeof(bones[i].name)))return RF_FORMAT;
    for(i=0;i<count;++i)if(strstr(bones[i].name,"spine")) {
        if(first!=-1){second=(int32_t)i;break;}first=(int32_t)i;
    }
    for(i=0;i<count;++i)if(strstr(bones[i].name,"head")){head=(int32_t)i;break;}
    parent=second<0?(int32_t)count:bones[second].parent;
    if(parent!=first){int32_t swap=first;first=second;second=swap;}
    out[0]=first;out[1]=second;out[2]=head;return RF_OK;
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
/* 518e10: basis conversion without quaternion normalization. */
int rf_model_basis_rotation(const float basis[9],float out[4])
{
    float q[4],diagonal_pair;double trace,root,scale;unsigned i,axis;
    if(!basis || !out)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_FORMAT;
    diagonal_pair=(float)((double)basis[4]+basis[8]);
    trace=((double)basis[4]+basis[8])+basis[0];
    if(trace>=0) {
        root=sqrt(trace+1);q[3]=(float)(root*.5);scale=.5/root;
        q[0]=(float)(((double)basis[7]-basis[5])*scale);
        q[1]=(float)(((double)basis[2]-basis[6])*scale);
        q[2]=(float)(((double)basis[3]-basis[1])*scale);
    } else {
        axis=basis[0]<basis[4]?1:0;if(basis[axis*4]<basis[8])axis=2;
        /* Only the X branch reloads the rounded diagonal-pair spill. */
        if(axis==0)root=sqrt(((double)basis[0]-diagonal_pair)+1);
        else if(axis==1)root=sqrt(((double)basis[4]-((double)basis[0]+basis[8]))+1);
        else root=sqrt(((double)basis[8]-((double)basis[4]+basis[0]))+1);
        q[axis]=(float)(root*.5);scale=.5/root;
        if(axis==0) {
            q[1]=(float)(((double)basis[3]+basis[1])*scale);
            q[2]=(float)(((double)basis[6]+basis[2])*scale);
            q[3]=(float)(((double)basis[7]-basis[5])*scale);
        } else if(axis==1) {
            q[2]=(float)(((double)basis[7]+basis[5])*scale);
            q[0]=(float)(((double)basis[3]+basis[1])*scale);
            q[3]=(float)(((double)basis[2]-basis[6])*scale);
        } else {
            q[0]=(float)(((double)basis[6]+basis[2])*scale);
            q[1]=(float)(((double)basis[7]+basis[5])*scale);
            q[3]=(float)(((double)basis[3]-basis[1])*scale);
        }
    }
    for(i=0;i<4;++i)if(!isfinite(q[i]))return RF_RANGE;
    memcpy(out,q,sizeof(q));return RF_OK;
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
    if(!isfinite(t))return RF_FORMAT;
    while(t<0) {float next=t+1;if(next==t)return RF_RANGE;t=next;}
    while(t>1) {float next=t-1;if(next==t)return RF_RANGE;t=next;}

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
int rf_model_override_pose(float matrix[12],const float basis[9],float weight)
{
    float target[4],current[4],blended[4],result[12];int status;unsigned i;
    if(!matrix || !basis)return RF_RANGE;
    if(!isfinite(weight))return RF_FORMAT;
    for(i=0;i<12;++i)if(!isfinite(matrix[i]))return RF_FORMAT;
    status=rf_model_basis_rotation(basis,target);if(status)return status;
    status=rf_model_basis_rotation(matrix,current);if(status)return status;
    status=pose_interpolate(current,target,weight,blended);if(status)return status;
    status=rf_model_attachment_transform(blended,matrix+9,result);if(status)return status;
    memcpy(matrix,result,sizeof(result));return RF_OK;
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
                             uint32_t resource_count, float root_displacement[3], float (*matrices)[12], uint16_t *generations, uint32_t capacity,const rf_model_bone_override *overrides)
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
        if(overrides && overrides[index].enabled) {
            status=rf_model_override_pose(matrices[index],overrides[index].basis,overrides[index].weight);
            if(status)return status;
        }
    }
    return RF_OK;
}

int rf_model_sample_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                             const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                             uint32_t resource_count, float root_displacement[3], float (*matrices)[12], uint32_t capacity)
{
    return model_sample_playback(bones,count,state,motions,resources,resource_count,root_displacement,matrices,NULL,capacity,NULL);
}

int rf_model_evaluate_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                               const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                               uint32_t resource_count, float root_displacement[3], float (*matrices)[12],
                               uint16_t *generations, uint32_t capacity)
{
    if (!generations) return RF_RANGE;
    return model_sample_playback(bones,count,state,motions,resources,resource_count,root_displacement,matrices,generations,capacity,NULL);
}

int rf_model_evaluate_overrides(const rf_model_bone *bones,uint32_t count,const rf_motion_playback_state *state,
    const rf_motion_file *const *motions,const rf_motion_playback_resource *resources,uint32_t resource_count,
    float root_displacement[3],float (*matrices)[12],uint16_t *generations,uint32_t capacity,
    const rf_model_bone_override *overrides)
{
    if(!generations)return RF_RANGE;
    return model_sample_playback(bones,count,state,motions,resources,resource_count,root_displacement,matrices,generations,capacity,overrides);
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

int rf_model_query_bone(const float (*pose)[12],uint32_t count,int32_t index,rf_model_bone_query *out)
{
    rf_model_bone_query value={{0},{1,0,0,0,1,0,0,0,1}};unsigned i;
    if(!out || count>256 || index<-1 || (index>=0 && ((uint32_t)index>=count || !pose)))return RF_RANGE;
    if(index>=0) {
        for(i=0;i<12;++i)if(!isfinite(pose[index][i]))return RF_FORMAT;
        memcpy(value.basis,pose[index],36);memcpy(value.position,pose[index]+9,12);
    }
    *out=value;return RF_OK;
}

int rf_model_object_follow_point(const float (*pose)[12],uint32_t count,int32_t index,
    const float orientation[9],const float position[3],float out[3])
{
    rf_model_bone_query bone;float result[3];unsigned i;int status;
    if(!position || !out)return RF_RANGE;
    if(index==-1) {memmove(out,position,12);return RF_OK;}
    if(!orientation)return RF_RANGE;
    status=rf_model_query_bone(pose,count,index,&bone);if(status)return status;
    for(i=0;i<9;++i)if(!isfinite(orientation[i]))return RF_FORMAT;
    for(i=0;i<3;++i) {
        float rotated;
        if(!isfinite(position[i]))return RF_FORMAT;
        /*4faa90 sums Z then Y then X, stores binary32 before 40a030 adds
         * the object translation. This differs from 5034f0 tag placement. */
        rotated=(float)(((double)bone.position[2]*orientation[i+6]+
                         (double)bone.position[1]*orientation[i+3])+
                         (double)bone.position[0]*orientation[i]);
        result[i]=position[i]+rotated;
        if(!isfinite(result[i]))return RF_RANGE;
    }
    memcpy(out,result,12);return RF_OK;
}
