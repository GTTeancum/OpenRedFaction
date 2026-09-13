#include "rf/effect.h"
#include "rf/lightmap.h"
#include "rf/geometry.h"
#include "rf/glare.h"
#include "rf/particle_pool.h"
#include "rf/visibility.h"
#include "rf/level_particles.h"
#include "rf/entity_assets.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
static int queue_parent_lookup(void *context,uint32_t handle,rf_level_particle_object *out)
{
    memset(out,0,sizeof(*out));out->uid=-1;
    if(handle)*out=*(const rf_level_particle_object*)context;
    return RF_OK;
}
static uint32_t move_trace[9],move_room;static int move_status;
static int move_locate(void *ctx,uint32_t old,const float from[3],const float to[3],uint32_t flags,uint32_t *room)
{(void)ctx;move_trace[0]++;move_trace[1]=old;memcpy(move_trace+2,from,12);memcpy(move_trace+5,to,12);move_trace[8]=flags;*room=move_room;return move_status;}
typedef struct render_dispatch_fixture {uint32_t wire[6],trace[8],count;} render_dispatch_fixture;
static int render_dispatch_record(render_dispatch_fixture *f,uint32_t code)
{f->trace[f->count++]=code;return f->wire[5]==f->count?RF_IO:RF_OK;}
static int render_dispatch_white(void *c){return render_dispatch_record(c,1);}
static int render_dispatch_kind(void *c,uint32_t model,uint32_t *out){render_dispatch_fixture *f=c;(void)model;*out=f->wire[3];return render_dispatch_record(f,3);}
static int render_dispatch_prepare(void *c,uint32_t model){(void)model;return render_dispatch_record(c,4);}
static int render_dispatch_render(void *c,uint32_t kind){return render_dispatch_record(c,5+kind);}
static uint32_t glare_refresh_value,glare_refresh_calls;static int glare_refresh_error;
static int glare_refresh_search(void *context,rf_glare_base_owner *owner,const float camera[3],uint32_t *result)
{(void)context;(void)owner;(void)camera;++glare_refresh_calls;*result=glare_refresh_value;return glare_refresh_error;}
typedef struct corona_tail_fixture {rf_glare_base_owner *owner;uint32_t count,fail,trace[64];} corona_tail_fixture;
static int corona_record(corona_tail_fixture *c,uint32_t record[8])
{memcpy(c->trace+c->count*8,record,32);++c->count;return c->fail==c->count?RF_IO:RF_OK;}
static int corona_color(void *context,uint32_t r,uint32_t g,uint32_t b,uint32_t a)
{uint32_t record[8]={2,r,g,b,a};return corona_record(context,record);}
static int corona_texture(void *context,uint32_t bitmap,int32_t second)
{uint32_t record[8]={3,bitmap,(uint32_t)second};return corona_record(context,record);}
static int corona_billboard(void *context,const float position[3],float angle,float size,uint32_t mode)
{corona_tail_fixture *c=context;uint32_t record[8]={4};if(position!=c->owner->position)return RF_RANGE;
 memcpy(record+1,&angle,4);memcpy(record+2,&size,4);record[3]=mode;return corona_record(c,record);}
static int corona_oriented(void *context,const float first[3],const float second[3],float size,uint32_t mode)
{corona_tail_fixture *c=context;uint32_t record[8]={5};if(first!=c->owner->state.vectors[0] || second!=c->owner->state.vectors[1])return RF_RANGE;
 memcpy(record+2,&size,4);record[3]=mode;return corona_record(c,record);}
typedef struct corona_frame_fixture {corona_tail_fixture graphics;uint32_t parent,visible,special;} corona_frame_fixture;
static int corona_parent(void *context,uint32_t handle,uint32_t *visible)
{corona_frame_fixture *c=context;uint32_t record[8]={6,handle};*visible=c->parent!=2;return corona_record(&c->graphics,record);}
static int corona_search(void *context,rf_glare_base_owner *owner,const float camera[3],uint32_t *visible)
{corona_frame_fixture *c=context;uint32_t record[8]={7,owner->handle};(void)camera;*visible=c->visible;return corona_record(&c->graphics,record);}
static int corona_special(void *context,rf_glare_base_owner *owner,const float camera[3],uint32_t *visible)
{corona_frame_fixture *c=context;uint32_t record[8]={8,1};(void)owner;(void)camera;*visible=c->special==1;return corona_record(&c->graphics,record);}
static int corona_flash(void *context,uint32_t r,uint32_t g,uint32_t b,int32_t alpha)
{corona_frame_fixture *c=context;uint32_t record[8]={9,r,g,b,(uint32_t)alpha};return corona_record(&c->graphics,record);}
typedef struct attachment_probe_context {
    rf_attachment_node nodes[16];uint32_t flags[16],count,events[96];
} attachment_probe_context;
typedef struct volume_frame_fixture {corona_tail_fixture trace;uint32_t parent;} volume_frame_fixture;
static int volume_special(void *c,rf_glare_base_owner *o,uint32_t *allowed)
{(void)c;(void)o;*allowed=0;return RF_OK;}
static int volume_parent(void *context,uint32_t handle,uint32_t *visible)
{volume_frame_fixture *c=context;uint32_t record[8]={1,handle};*visible=c->parent!=2;return corona_record(&c->trace,record);}
static int volume_actor(void *context,rf_glare_base_owner *o,float *length,float *width,uint32_t *draw)
{volume_frame_fixture *c=context;uint32_t record[8]={2,o->parent_handle};(void)length;(void)width;(void)draw;return corona_record(&c->trace,record);}
static int volume_enable(void *context,uint32_t enabled)
{volume_frame_fixture *c=context;uint32_t record[8]={3,enabled};return corona_record(&c->trace,record);}
static int volume_color(void *context,uint32_t r,uint32_t g,uint32_t b,uint32_t a)
{volume_frame_fixture *c=context;uint32_t record[8]={4,r,g,b,a};return corona_record(&c->trace,record);}
static int volume_texture(void *context,uint32_t bitmap,int32_t second)
{volume_frame_fixture *c=context;uint32_t record[8]={5,bitmap,(uint32_t)second};return corona_record(&c->trace,record);}
static int volume_beam(void *context,const float end[3],const float start[3],float width,uint32_t mode)
{volume_frame_fixture *c=context;uint32_t record[8]={6};
 if(start!=c->trace.owner->position)return RF_RANGE;memcpy(record+1,end,12);memcpy(record+4,&width,4);record[5]=mode;return corona_record(&c->trace,record);}
static rf_attachment_node *attachment_probe_lookup(void *context,uint32_t handle)
{
    attachment_probe_context *c=context;uint32_t *e=c->events+c->count++*3;
    e[0]=0;e[1]=handle;e[2]=0;return handle<16?c->nodes+handle:NULL;
}
static int attachment_probe_publish(void *context,rf_attachment_node *node,rf_attachment_node *parent)
{
    attachment_probe_context *c=context;uint32_t *e=c->events+c->count++*3;
    e[0]=1;e[1]=(uint32_t)(node-c->nodes);e[2]=(uint32_t)(parent-c->nodes);return RF_OK;
}
typedef struct volume_actor_fixture {
    rf_glare_volume_actor parent,target;uint32_t parent_present,target_present,count,trace[2];
} volume_actor_fixture;
static int volume_actor_lookup(void *context,uint32_t handle,const rf_glare_volume_actor **out)
{
    volume_actor_fixture *c=context;if(c->count>=2)return RF_RANGE;c->trace[c->count++]=handle;
    *out=handle==32 && c->parent_present?&c->parent:handle==64 && c->target_present?&c->target:NULL;return RF_OK;
}
static int volume_special_actor_flags(void *context,uint32_t handle,uint32_t *present,uint32_t *flags)
{uint32_t *v=context;if(handle!=32)return RF_RANGE;++v[2];*present=v[0];*flags=v[1];return v[3]?RF_IO:RF_OK;}
typedef struct segment_create_fixture {
    rf_glare_base_owner owner;rf_glare_create_descriptor descriptor;uint32_t count,null_owner,error,offset;
} segment_create_fixture;
static int segment_create_tag(void *context,uint32_t parent,int32_t tag,float pose[12])
{
    segment_create_fixture *c=context;static const float first[12]={1,0,0,0,1,0,0,0,1,1.25f,-2.5f,3.75f};
    static const float second[12]={0,0,-1,0,1,0,1,0,0,5.25f,1.5f,-.25f};
    if(parent!=123 || (tag!=7 && tag!=9))return RF_RANGE;++c->count;
    memcpy(pose,tag==7?first:second,48);if(tag==9)pose[9]+=(float)c->offset;
    return c->count==c->error?RF_IO:RF_OK;
}
static int segment_create_allocate(void *context,const rf_glare_create_descriptor *d,rf_glare_base_owner **out)
{
    segment_create_fixture *c=context;++c->count;c->descriptor=*d;
    if(c->count==c->error)return RF_IO;*out=c->null_owner?NULL:&c->owner;return RF_OK;
}
static uint32_t light_dirty_trace[66],light_dirty_fail;
static int light_dirty_bounds(void *context,const float minimum[3],const float maximum[3],uint32_t *hit)
{
    uint32_t id=(uint32_t)minimum[0];(void)context;(void)maximum;
    light_dirty_trace[1+light_dirty_trace[0]++]=id;
    if(light_dirty_fail==light_dirty_trace[0])return RF_IO;
    *hit=(uint32_t)minimum[1];return RF_OK;
}
static uint32_t light_dispatch_trace[85],light_dispatch_fail;
static int light_dispatch_record(uint32_t code,uint32_t a,uint32_t b,uint32_t c)
{uint32_t n=light_dispatch_trace[0]++;light_dispatch_trace[1+n*4]=code;light_dispatch_trace[2+n*4]=a;light_dispatch_trace[3+n*4]=b;light_dispatch_trace[4+n*4]=c;return light_dispatch_fail==n+1?RF_IO:RF_OK;}
static int light_dispatch_dirty(void *c,uint32_t a,uint32_t b,uint32_t d){(void)c;return light_dispatch_record(1,a,b,d);}
static int light_dispatch_enter(void *c,uint32_t a,uint32_t b){(void)c;return light_dispatch_record(2,a,b,0);}
static int light_dispatch_transform(void *c,uint32_t a){(void)c;return light_dispatch_record(3,a,0,0);}
static int light_dispatch_leave(void *c){(void)c;return light_dispatch_record(4,0,0,0);}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--light-storage")) {
        struct {uint32_t mappings[8],order[8];rf_collision_face faces[8];} input;
        unsigned char records[8][56];uint32_t offsets[8],i,status,budget;rf_geometry geometry;rf_light_dirty_storage *owner=NULL;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&geometry,0,sizeof(geometry));memset(records,0,sizeof(records));geometry.data=records[0];geometry.faces=8;geometry.mappings=16;geometry.face_offsets=offsets;
            for(i=0;i<8;i++){offsets[i]=i*56;memcpy(records[i]+20,input.mappings+i,4);}
            budget=sizeof(*owner)+8*sizeof(*owner->faces)+16;
            status=rf_visibility_light_storage_open(&geometry,input.faces,input.order,8,budget-1,&owner);if(!status || owner)return 9;
            status=rf_visibility_light_storage_open(&geometry,input.faces,input.order,8,budget,&owner);fwrite(&status,4,1,stdout);
            if(!status){memset(&geometry,0xa5,sizeof(geometry));memset(records,0xa5,sizeof(records));memset(&input,0xa5,sizeof(input));fwrite(&owner->face_count,12,1,stdout);fwrite(owner->faces,36,8,stdout);fwrite(owner->dirty,16,1,stdout);}
            rf_visibility_light_storage_close(&owner);rf_visibility_light_storage_close(&owner);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--view-stack")) {
        struct {rf_visibility_view_state state;float poses[3][12];} input;
        rf_visibility_view_state saved[3];rf_visibility_view_stack stack;uint32_t i,status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&stack,0,sizeof(stack));stack.current=input.state;stack.saved=saved;stack.capacity=3;stack.color_marker=19;
            for(i=0;i<6;i++) {
                status=i<3?rf_visibility_view_push(&stack,input.poses[i],input.poses[i]+3):rf_visibility_view_pop(&stack);
                fwrite(&status,4,1,stdout);fwrite(&stack.current,96,1,stdout);fwrite(&stack.depth,4,1,stdout);fwrite(&stack.color_marker,1,1,stdout);
            }
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-dispatch")) {
        uint32_t input[6],status,i;rf_light_update_view views[5],*head;
        rf_light_update_backend backend={light_dispatch_dirty,light_dispatch_enter,light_dispatch_transform,light_dispatch_leave,NULL};
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            if(input[3]>5)return 9;
            for(i=0;i<input[3];i++){views[i].solid=20+i;views[i].position=30+i;views[i].matrix=40+i;views[i].next=i+1<input[3]?views+i+1:input[4]?views:NULL;}
            head=input[3]?views:NULL;memset(light_dispatch_trace,0,sizeof(light_dispatch_trace));light_dispatch_fail=input[5];
            status=rf_visibility_light_dispatch(input[0],7,input[1],input[2],&head,5,&backend);
            fwrite(&status,4,1,stdout);fwrite(light_dispatch_trace,sizeof(light_dispatch_trace),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-texel-coverage")) {
        struct {float minimum[2],maximum[2];uint32_t seen,count,counts[4];float uv[4][8][2];} input;
        rf_lightmap_uv_polygon polygons[4];uint32_t status,seen,hits,i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            seen=input.seen;hits=0xa5a5a5a5u;
            for(i=0;i<4;i++){polygons[i].uv=input.uv[i];polygons[i].count=input.counts[i];}
            status=RF_RANGE;
            if(input.count<=4 && input.counts[0]<=8 && input.counts[1]<=8 && input.counts[2]<=8 && input.counts[3]<=8)
                status=rf_lightmap_texel_coverage(polygons,input.count,input.minimum,input.maximum,&seen,&hits);
            fwrite(&status,4,1,stdout);fwrite(&seen,4,1,stdout);fwrite(&hits,4,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-interpolate-edges")) {
        struct {rf_lightmap_sample_vertex vertices[4];float center[2];} input;
        rf_lightmap_special_sample sample;uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&sample,0xa5,sizeof(sample));status=rf_lightmap_interpolate_edges(input.vertices,input.center,&sample);
            fwrite(&status,4,1,stdout);fwrite(&sample,sizeof(sample),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-select-sample")) {
        struct {float center[2],radius;uint32_t count,counts[4];rf_lightmap_sample_vertex vertices[4][8];} input;
        rf_lightmap_sample_polygon polygons[4];rf_lightmap_special_sample sample;uint32_t status,kind,i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&sample,0xa5,sizeof(sample));kind=0xa5a5a5a5u;status=RF_RANGE;
            for(i=0;i<4;i++){polygons[i].vertices=input.vertices[i];polygons[i].count=input.counts[i];}
            if(input.count<=4 && input.counts[0]<=8 && input.counts[1]<=8 && input.counts[2]<=8 && input.counts[3]<=8)
                status=rf_lightmap_select_sample(polygons,input.count,input.center,input.radius,&sample,&kind);
            fwrite(&status,4,1,stdout);fwrite(&kind,4,1,stdout);fwrite(&sample,sizeof(sample),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-special-border")) {
        struct {uint32_t width,height,capacity;float planes[3][256];} input;
        float *channels[3];uint32_t status,i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            for(i=0;i<3;i++)channels[i]=input.planes[i];
            status=input.capacity>256?RF_RANGE:rf_lightmap_copy_special_border(channels,input.width,input.height,input.capacity);
            fwrite(&status,4,1,stdout);fwrite(input.planes,sizeof(input.planes),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-special-grid")) {
        struct {uint32_t image_width,image_height,x,y,width,height,count,masked,polygon_count,counts[4];
            rf_lightmap_sample_vertex vertices[4][8];rf_vfx_light_source lights[4];float planes[3][64];unsigned char masks[4][64];} input;
        rf_lightmap_sample_lighting view;rf_lightmap_sample_polygon polygons[4];const unsigned char *masks[4];uint32_t status,i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&view,0,sizeof(view));view.sample.image_width=input.image_width;view.sample.image_height=input.image_height;
            view.sample.x=input.x;view.sample.y=input.y;view.width=input.width;view.height=input.height;
            view.lights=input.lights;view.light_count=input.count;view.directional_scale=.25f;view.capacity=64;view.mask_bytes=64;
            for(i=0;i<4;i++){polygons[i].vertices=input.vertices[i];polygons[i].count=input.counts[i];masks[i]=input.masks[i];}
            view.masks=input.masked?masks:NULL;for(i=0;i<3;i++)view.channels[i]=input.planes[i];status=RF_RANGE;
            if(input.count<=4 && input.polygon_count<=4 && input.counts[0]<=8 && input.counts[1]<=8 && input.counts[2]<=8 && input.counts[3]<=8)
                status=rf_lightmap_accumulate_special(&view,polygons,input.polygon_count);
            fwrite(&status,4,1,stdout);fwrite(input.planes,sizeof(input.planes),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--geometry-mappings")) {
        uint32_t header[4],i,status;rf_geometry geometry;rf_lightmap_mapping mapping;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(header,sizeof(header),1,stdin)==1) {
            memset(&geometry,0,sizeof(geometry));geometry.bytes=header[0];geometry.mapping_offset=header[2];geometry.mappings=header[3];
            if(geometry.bytes>16*1024*1024)return 2;
            geometry.data=(unsigned char *)malloc(geometry.bytes);if(!geometry.data)return 2;
            if(fread(geometry.data,geometry.bytes,1,stdin)!=1){free(geometry.data);return 2;}
            for(i=0;i<geometry.mappings;i++) {
                memset(&mapping,0xa5,sizeof(mapping));status=rf_geometry_get_lightmap_mapping(&geometry,i,header[1],&mapping);
                fwrite(&status,4,1,stdout);fwrite(&mapping,sizeof(mapping),1,stdout);
            }
            free(geometry.data);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-seed-ambient")) {
        struct {uint32_t width,height,capacity;float global[3];unsigned char room[4];} input;
        rf_lightmap_sample_lighting view;float planes[3][256];uint32_t status,c;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&view,0,sizeof(view));memset(planes,0xa5,sizeof(planes));view.width=input.width;view.height=input.height;view.capacity=input.capacity;
            for(c=0;c<3;c++)view.channels[c]=planes[c];
            status=input.capacity>256?RF_RANGE:rf_lightmap_seed_ambient(&view,input.global,input.room);
            fwrite(&status,4,1,stdout);fwrite(planes,sizeof(planes),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-fill-ambient")) {
        struct {uint32_t width,height,pitch,offset,dirty;float global[3];unsigned char room[4];} input;
        unsigned char rgb[1024],dirty;uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(rgb,0xa5,sizeof(rgb));dirty=(unsigned char)input.dirty;
            status=input.offset>sizeof(rgb)?RF_RANGE:rf_lightmap_fill_ambient(rgb+input.offset,sizeof(rgb)-input.offset,input.pitch,input.width,input.height,input.global,input.room,&dirty);
            fwrite(&status,4,1,stdout);fwrite(&dirty,1,1,stdout);fwrite(rgb,sizeof(rgb),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-edge-crossing")) {
        float input[8];uint32_t status,hit;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            hit=0xa5a5a5a5u;status=rf_lightmap_edge_crossing(input,input+2,input+4,input+6,&hit);
            fwrite(&status,4,1,stdout);fwrite(&hit,4,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-corner-normal")) {
        struct {rf_lightmap_normal_face base,adjacent[8];uint32_t count;} input;float output[3];uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(output,0xa5,sizeof(output));status=input.count>8?RF_RANGE:rf_lightmap_corner_normal(&input.base,input.adjacent,input.count,output);
            fwrite(&status,4,1,stdout);fwrite(output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-mapping-read")) {
        struct {uint32_t count;unsigned char record[96];} input;rf_lightmap_mapping output;uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));status=rf_lightmap_mapping_read(input.record,96,input.count,&output);
            fwrite(&status,4,1,stdout);fwrite(&output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-accumulate-grid")) {
        struct {rf_lightmap_sample_plane sample;uint32_t width,height,count,masked;float directional;rf_vfx_light_source lights[4];float channels[3][64];unsigned char masks[4][64];} input;
        rf_lightmap_sample_lighting view;const unsigned char *masks[4];uint32_t status,i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            view.sample=input.sample;view.width=input.width;view.height=input.height;view.lights=input.lights;view.light_count=input.count;
            for(i=0;i<4;i++)masks[i]=input.masks[i];view.masks=input.masked?masks:NULL;view.mask_bytes=64;view.directional_scale=input.directional;
            for(i=0;i<3;i++)view.channels[i]=input.channels[i];view.capacity=64;
            status=input.count>4?RF_RANGE:rf_lightmap_accumulate_samples(&view);
            fwrite(&status,4,1,stdout);fwrite(input.channels,sizeof(input.channels),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-sample-position")) {
        struct {rf_lightmap_sample_plane view;uint32_t x,y;} input;float output[3];uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(output,0xa5,sizeof(output));status=rf_lightmap_sample_position(&input.view,input.x,input.y,output);
            fwrite(&status,4,1,stdout);fwrite(output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-accumulate")) {
        struct {uint32_t count,weighted;float position[3],normal[3],initial[3],directional;rf_vfx_light_source lights[16];unsigned char weights[16];} input;
        float output[3];uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(output,0xa5,sizeof(output));status=input.count>16?RF_RANGE:rf_vfx_light_accumulate(input.position,input.normal,input.initial,input.directional,input.lights,input.count,(input.weighted&1)?input.weights:NULL,input.weighted>>1,output);
            fwrite(&status,4,1,stdout);fwrite(output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-resolve-rgb")) {
        struct {uint32_t width,height,pitch,offset,dirty;float channels[3][256];} input;
        unsigned char output[1536],dirty;uint32_t status;rf_lightmap_accumulation view;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            view.channels[0]=input.channels[0];view.channels[1]=input.channels[1];view.channels[2]=input.channels[2];
            view.count=256;view.width=input.width;view.height=input.height;memset(output,0xa5,sizeof(output));dirty=(unsigned char)input.dirty;
            status=input.offset>sizeof(output)?RF_RANGE:rf_lightmap_resolve_rgb(&view,output+input.offset,sizeof(output)-input.offset,input.pitch,&dirty);
            fwrite(&status,4,1,stdout);fwrite(&dirty,1,1,stdout);fwrite(output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-filtered-rgb")) {
        struct {uint32_t width,height,x,y;float channels[3][256];} input;
        unsigned char output[3];uint32_t status;rf_lightmap_accumulation view;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            view.channels[0]=input.channels[0];view.channels[1]=input.channels[1];view.channels[2]=input.channels[2];
            view.count=256;view.width=input.width;view.height=input.height;memset(output,0xa5,3);
            status=rf_lightmap_filtered_rgb(&view,input.x,input.y,output);
            fwrite(&status,4,1,stdout);fwrite(output,3,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-accumulated-rgb")) {
        float input[3];unsigned char output[3];uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            memset(output,0xa5,3);status=rf_lightmap_accumulated_rgb(input,output);
            fwrite(&status,4,1,stdout);fwrite(output,3,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-rgb-upload")) {
        struct {uint32_t x,y,width,height,source_width,pitch,flags,locked;unsigned char rgb[768];} input;
        uint32_t status;unsigned char dirty,packed[768];rf_lightmap_rgb_upload view;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(packed,0xa5,sizeof(packed));dirty=(unsigned char)input.flags;
            view.rgb=input.rgb;view.rgb_bytes=sizeof(input.rgb);view.rgb_pitch=input.source_width*3;
            view.packed=input.locked?packed:NULL;view.packed_bytes=sizeof(packed);view.packed_pitch=input.pitch;
            view.x=input.x;view.y=input.y;view.width=input.width;view.height=input.height;
            status=rf_lightmap_upload_rgb_1555(&view,&dirty);
            fwrite(&status,4,1,stdout);fwrite(&dirty,1,1,stdout);fwrite(packed,sizeof(packed),1,stdout);
        }
        return 0;
    }
    if(argc==2 && (!strcmp(argv[1],"--light-dirty-solid") || !strcmp(argv[1],"--light-dirty-world"))) {
        struct {rf_vfx_light_definition definition;uint32_t mode,update,path;float bounds[6];rf_light_dirty_room rooms[2];rf_collision_node nodes[3];rf_light_dirty_face faces[3];unsigned char dirty[4];} input;
        uint32_t status,primary[1]={0},children[1]={1},roots[4],stack[4];rf_light_dirty_solid solid;rf_light_visibility_volume volume;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&solid,0,sizeof(solid));memcpy(solid.minimum,input.bounds,24);solid.rooms=input.rooms;solid.room_count=2;
            solid.primary=primary;solid.primary_count=input.path;solid.children=children;solid.child_count=1;solid.nodes=input.nodes;solid.node_count=3;
            solid.faces=input.faces;solid.face_count=3;solid.dirty=input.dirty;solid.dirty_count=4;solid.roots=roots;solid.root_capacity=4;solid.stack=stack;solid.stack_capacity=4;
            status=rf_visibility_light_volume(&input.definition,&volume);
            if(!status && !strcmp(argv[1],"--light-dirty-world")) {
                rf_geometry_collision_world world={0};rf_geometry_collision_room rooms[2]={0};rf_collision_room_view views[2]={0};
                rf_light_dirty_storage storage={0};rf_light_dirty_room selected[2];uint32_t offsets[2],j;
                rf_light_world_scratch scratch={selected,offsets,roots,2,4};
                memcpy(world.minimum,input.bounds,24);world.rooms=rooms;world.views=views;world.room_count=2;
                world.primary=primary;world.primary_count=input.path;world.children=children;world.child_count=1;
                for(j=0;j<2;j++) {memcpy(rooms[j].minimum,input.rooms[j].minimum,24);views[j].first_child=input.rooms[j].first_child;views[j].child_count=input.rooms[j].child_count;
                    rooms[j].tree.nodes=input.nodes+(j?2:0);rooms[j].tree.node_count=j?1:2;rooms[j].tree.face_count=j?1:2;rooms[j].tree.stack=stack;rooms[j].tree.node_capacity=4;}
                input.nodes[2].first_face=0;
                storage.faces=input.faces;storage.face_count=3;storage.dirty=input.dirty;storage.dirty_count=4;
                status=rf_visibility_light_world(&volume,&world,&storage,input.mode,input.update,&scratch);
            } else if(!status)status=rf_visibility_light_solid(&volume,&solid,input.mode,input.update);
            fwrite(&status,4,1,stdout);fwrite(input.faces,sizeof(input.faces),1,stdout);fwrite(input.dirty,4,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-volume-view")) {
        struct {rf_vfx_light_definition definition;float minimum[3],maximum[3],origin[3],basis[9];} input;
        struct {uint32_t status;rf_light_visibility_volume volume;uint32_t hit;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));output.status=rf_visibility_light_volume_view(&input.definition,input.origin,input.basis,&output.volume);
            if(!output.status)output.status=rf_visibility_light_bounds(&output.volume,input.minimum,input.maximum,&output.hit);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-volume")) {
        struct {rf_vfx_light_definition definition;float minimum[3],maximum[3];} input;
        struct {uint32_t status;rf_light_visibility_volume volume;uint32_t hit;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));output.status=rf_visibility_light_volume(&input.definition,&output.volume);
            if(!output.status)output.status=rf_visibility_light_bounds(&output.volume,input.minimum,input.maximum,&output.hit);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-dirty-roots")) {
        struct {rf_light_dirty_room rooms[8];uint32_t primary[3],children[6];} input;
        uint32_t output[11];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(output,0xa5,sizeof(output));memset(light_dirty_trace,0,sizeof(light_dirty_trace));light_dirty_fail=0;
            output[0]=rf_visibility_light_roots(input.rooms,8,input.primary,3,input.children,6,output+2,9,output+1,light_dirty_bounds,NULL);
            fwrite(output,sizeof(output),1,stdout);fwrite(light_dirty_trace,sizeof(light_dirty_trace),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-dirty-tree")) {
        struct {uint32_t mode,update;rf_collision_node nodes[15];rf_light_dirty_face faces[15];unsigned char dirty[8];uint32_t roots[2];} input;
        uint32_t status,stack[15];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(light_dirty_trace,0,sizeof(light_dirty_trace));light_dirty_fail=0;
            status=rf_visibility_light_tree(input.nodes,15,input.roots,2,input.faces,15,input.dirty,8,input.mode,input.update,stack,15,light_dirty_bounds,NULL);
            fwrite(&status,4,1,stdout);fwrite(input.faces,sizeof(input.faces),1,stdout);fwrite(input.dirty,8,1,stdout);fwrite(light_dirty_trace,sizeof(light_dirty_trace),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-dirty-faces")) {
        struct {uint32_t mode,update,fail;rf_light_dirty_face faces[16];unsigned char dirty[8];} input;
        uint32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(light_dirty_trace,0,sizeof(light_dirty_trace));light_dirty_fail=input.fail;
            status=rf_visibility_light_faces(input.faces,16,input.dirty,8,input.mode,input.update,light_dirty_bounds,NULL);
            fwrite(&status,4,1,stdout);fwrite(input.faces,sizeof(input.faces),1,stdout);fwrite(input.dirty,8,1,stdout);fwrite(light_dirty_trace,18*4,1,stdout);
        }
        return 0;
    }
    if(argc==2 && (!strcmp(argv[1],"--light-cone-planes") || !strcmp(argv[1],"--light-cone-prepare"))) {
        float input[8];struct {uint32_t status;rf_visibility_plane planes[6];} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));output.status=(!strcmp(argv[1],"--light-cone-prepare")?rf_visibility_light_cone_prepare:rf_visibility_light_cone_planes)(input,input+3,input[6],input[7],output.planes);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--light-cone-box")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        struct {float minimum[3],maximum[3];rf_visibility_plane planes[6];} input;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t output[2]={0,0xa5a5a5a5};
            output[0]=rf_visibility_light_cone_box(input.planes,input.minimum,input.maximum,output+1);
            fwrite(output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--glare-segment-create")) {
        uint32_t in[4],out[45];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            segment_create_fixture c;rf_glare_base_owner *owner=NULL;rf_object_list list;rf_glare_state *s=&c.owner.state;
            rf_glare_class classes[3]={{.5f,1,(void*)1},{1,.5f,(void*)1},{1,1,(void*)1}};
            rf_glare_segment_backend b={segment_create_tag,segment_create_allocate,&c};
            memset(&c,0,sizeof(c));memset(&c.owner,0xa5,sizeof(c.owner));rf_object_list_init(&list);
            c.null_owner=in[1];c.error=in[2];c.offset=in[3];
            out[0]=(uint32_t)rf_glare_segment_create(classes,3,(int32_t)in[0],123,7,9,&list,&b,&owner);
            out[1]=owner!=NULL;out[2]=c.count;out[3]=list.count;out[4]=c.owner.parent_handle;out[5]=s->parent;out[6]=(uint32_t)s->tag;
            out[7]=s->active;out[8]=s->reserved[0];out[9]=(uint32_t)s->occluder;out[10]=s->cached_solid;out[11]=s->cached_face;
            memcpy(out+12,s->samples,16);out[16]=s->definition==(void*)1;out[17]=(uint32_t)s->class_index;out[18]=s->flags;
            memcpy(out+19,s->last_position,12);out[22]=s->word_2cc;out[23]=s->byte_2d0;memcpy(out+24,s->vectors,24);
            out[30]=owner && list.sentinel.next==&s->link && list.sentinel.previous==&s->link && s->link.next==&list.sentinel && s->link.previous==&list.sentinel;
            memcpy(out+31,&c.descriptor,56);fwrite(out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--glare-segment-center")) {
        float in[6];struct {int32_t status;float center[3];} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){memset(&out,0xa5,sizeof(out));out.status=rf_glare_segment_center(in,in+3,out.center);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-special-gate")) {
        uint32_t in[6],out[3],context[4];rf_glare_base_owner owner;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&owner,0,sizeof(owner));owner.state.word_2cc=in[0];owner.flags=in[2];context[0]=in[3];context[1]=in[4];context[2]=0;context[3]=in[5];
            out[1]=0xa5a5a5a5;out[0]=(uint32_t)rf_glare_volume_special_allowed(&owner,1,in[1],32,volume_special_actor_flags,context,out+1);
            out[2]=context[2];fwrite(out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-actor-update")) {
        uint32_t in[31],out[8];float sizes[2],length,width;rf_random_state random;volume_actor_fixture c;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&c,0,sizeof(c));c.parent_present=in[0];c.parent.class_flags=in[1];c.parent.occupants=in+3;c.parent.occupant_count=3;
            c.target_present=in[6];c.target.flags=in[7];c.target.player_present=in[8];
            memcpy(c.parent.command,in+9,12);memcpy(c.target.command,in+12,12);memcpy(c.parent.position,in+15,12);memcpy(c.parent.basis,in+18,36);
            memcpy(&length,in+27,4);memcpy(&width,in+28,4);random.value=in[30];out[4]=in[29];memset(sizes,0xa5,8);
            out[0]=(uint32_t)rf_glare_volume_actor_update(32,in[2],length,width,volume_actor_lookup,&c,&random,sizes,out+4);
            out[1]=random.value;memcpy(out+2,sizes,8);out[5]=c.count;memcpy(out+6,c.trace,8);fwrite(out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-actor-aim")) {
        float in[15];double dot;uint32_t eligible;int32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&dot,0xa5,8);eligible=0xa5a5a5a5;status=rf_glare_volume_actor_aim(in,in+3,in+6,&dot,&eligible);
            fwrite(&status,4,1,stdout);fwrite(&dot,8,1,stdout);fwrite(&eligible,4,1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-actor-dimensions")) {
        uint32_t in[6];double dot;float length,width;rf_random_state random;struct {int32_t status;uint32_t random;float dimensions[2];uint32_t draw;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            memcpy(&dot,in,8);memcpy(&length,in+2,4);memcpy(&width,in+3,4);random.value=in[4];
            memset(&out,0xa5,sizeof(out));out.draw=in[5];
            out.status=rf_glare_volume_actor_dimensions(dot,length,width,&random,out.dimensions,&out.draw);out.random=random.value;
            fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-frame")) {
        uint32_t wire[15];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            rf_glare_base_owner owner={0};rf_glare_definition definition={0};rf_glare_volume_frame frame={0};uint32_t out[67];
            volume_frame_fixture fixture={{&owner,0,wire[14],{0}},wire[0]};
            rf_glare_volume_services services={volume_special,volume_parent,volume_actor,volume_enable,volume_color,volume_texture,volume_beam,&fixture};
            owner.parent_handle=32;owner.radius=13;memcpy(owner.position,wire+2,12);memcpy(owner.matrix+6,wire+5,12);
            memcpy(frame.camera,wire+8,12);frame.bitmap=(int32_t)wire[1];frame.mode=RF_PARTICLE_GLOW_MODE;
            memcpy(&definition.cone_degrees,wire+11,4);memcpy(&definition.height,wire+12,4);memcpy(&definition.length,wire+13,4);
            out[0]=(uint32_t)rf_glare_volume_render(&owner,&definition,&frame,&services);memcpy(out+1,&owner.radius,4);
            out[2]=fixture.trace.count;memcpy(out+3,fixture.trace.trace,256);fwrite(out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corona-frame")) {
        uint32_t wire[18];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,4,18,stdin)==18) {
            rf_glare_base_owner owner={0};rf_glare_definition definition={0};rf_glare_corona_frame frame={0};
            corona_frame_fixture fixture={{&owner,0,wire[17],{0}},wire[8],wire[9],wire[10]};
            rf_glare_corona_services services={corona_parent,corona_search,corona_special,corona_flash,&fixture,
                {corona_color,corona_texture,corona_billboard,corona_oriented,&fixture.graphics}};
            uint32_t output[73];int status;
            owner.handle=wire[0];owner.parent_handle=32;owner.radius=1;owner.state.active=(uint8_t)wire[2];
            owner.state.reserved[0]=(uint8_t)wire[3];owner.state.cached_face=wire[4];owner.state.word_2cc=wire[10]?1:0;
            owner.state.byte_2d0=(uint8_t)wire[11];memcpy(owner.position,wire+14,4);memcpy(owner.position+2,wire+13,4);
            owner.matrix[0]=owner.matrix[8]=-1;owner.matrix[4]=1;
            memcpy(owner.state.samples,wire+15,4);owner.state.samples[1]=owner.state.samples[0];
            memcpy(owner.state.samples+2,wire+16,4);owner.state.samples[3]=owner.state.samples[2];
            definition.cone_degrees=45;memcpy(&definition.intensity,wire+12,4);definition.radius_distance=definition.radius_scale=1;
            definition.color[0]=255;definition.color[1]=80;definition.color[2]=40;
            frame.basis[0]=frame.basis[4]=frame.basis[8]=1;frame.field_of_view=90;frame.intensity_scale=frame.size_scale=.5f;
            frame.frame=wire[1];frame.view=wire[7];frame.face_cache_state=(int32_t)wire[5];frame.bitmap=(int32_t)wire[6];
            status=rf_glare_corona_render(&owner,&definition,&frame,&services);output[0]=(uint32_t)status;
            memcpy(output+1,&owner.radius,4);memcpy(output+2,owner.state.samples,16);output[6]=owner.state.cached_face;
            output[7]=owner.state.reserved[0];output[8]=fixture.graphics.count;memcpy(output+9,fixture.graphics.trace,256);
            if(fwrite(output,4,73,stdout)!=73)return 3;
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--corona-camera")) {
        float wire[24];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,4,24,stdin)==24) {
            rf_glare_base_owner owner={0};float values[6];double angle;uint32_t output[9];int status;
            memcpy(owner.position,wire,12);memcpy(owner.matrix,wire+3,36);memset(values,0xa5,sizeof(values));memset(&angle,0xa5,8);
            status=rf_glare_corona_camera_setup(&owner,wire+12,wire+15,values,&angle);
            output[0]=(uint32_t)status;memcpy(output+1,values,24);memcpy(output+7,&angle,8);if(fwrite(output,4,9,stdout)!=9)return 3;
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--corona-attenuation")) {
        uint32_t wire[17];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,4,17,stdin)==17) {
            rf_glare_state state={0};rf_glare_definition definition={0};rf_glare_corona_environment environment;
            rf_glare_corona_values result;double angle;uint32_t output[5];int status;
            memcpy(&environment,wire,20);memcpy(&angle,wire+5,8);memcpy(&definition.cone_degrees,wire+7,20);
            memcpy(state.samples,wire+12,16);memset(&result,0xa5,sizeof(result));
            status=rf_glare_corona_attenuate(&state,wire[16],&definition,&environment,angle,&result);
            output[0]=(uint32_t)status;memcpy(output+1,&result,16);if(fwrite(output,4,5,stdout)!=5)return 3;
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--corona-tail")) {
        uint32_t wire[11];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,4,11,stdin)==11) {
            rf_glare_base_owner owner={0};rf_glare_corona_tail tail={0};corona_tail_fixture fixture={&owner,0,wire[10],{0}};
            rf_glare_corona_backend backend={corona_color,corona_texture,corona_billboard,corona_oriented,&fixture};
            uint32_t output[39];int status;float samples[4]={.1f,.2f,.3f,.4f};
            memcpy(&tail,wire,12);memcpy(&tail.side_dot,wire+4,4);tail.bitmap=wire[6];tail.view=wire[7];tail.draw=wire[8];
            memcpy(&owner.radius,wire+5,4);owner.state.byte_2d0=(uint8_t)wire[9];memcpy(owner.state.samples,samples,16);
            status=rf_glare_corona_submit(&owner,&tail,&backend);output[0]=(uint32_t)status;
            memcpy(output+1,&owner.radius,4);memcpy(output+2,owner.state.samples,16);output[6]=fixture.count;memcpy(output+7,fixture.trace,128);
            if(fwrite(output,4,39,stdout)!=39)return 3;
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--glare-fade")) {
        uint32_t wire[5];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,4,5,stdin)==5) {
            rf_glare_state state={0};float values[2]={1,1};uint32_t draw=0x12345678,output[8];int status;
            memcpy(state.samples,wire+1,16);status=rf_glare_fade_samples(&state,wire[0],values,&draw);
            output[0]=(uint32_t)status;memcpy(output+1,state.samples,16);memcpy(output+5,values,8);output[7]=draw;
            if(fwrite(output,4,8,stdout)!=8)return 3;
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--glare-refresh")) {
        uint32_t wire[7];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,4,7,stdin)==7) {
            rf_glare_base_owner owner={0};float camera[3]={0};uint32_t visible=0x12345678,output[5];int status;
            owner.handle=wire[0];owner.state.active=1;owner.state.cached_face=wire[3];owner.state.reserved[0]=(uint8_t)wire[4];
            glare_refresh_value=wire[5];glare_refresh_error=(int)wire[6];glare_refresh_calls=0;
            status=rf_glare_refresh_visibility(&owner,camera,wire[1],(int32_t)wire[2],glare_refresh_search,NULL,&visible);
            output[0]=(uint32_t)status;output[1]=owner.state.cached_face;output[2]=owner.state.reserved[0];output[3]=glare_refresh_calls;output[4]=visible;
            if(fwrite(output,4,5,stdout)!=5)return 3;
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--object-render-dispatch")) {
        render_dispatch_fixture f;rf_object_render_backend b={render_dispatch_white,render_dispatch_kind,render_dispatch_prepare,render_dispatch_render,&f};
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(f.wire,sizeof(f.wire),1,stdin)==1) {
            int status;f.count=0;memset(f.trace,0,sizeof(f.trace));
            status=rf_object_render_dispatch(f.wire+1,f.wire[0],f.wire[2],&b);
            fwrite(&status,4,1,stdout);fwrite(f.wire+1,4,1,stdout);fwrite(&f.count,4,1,stdout);fwrite(f.trace,4,8,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--emitter-move")) {
        uint32_t wire[15];rf_particle_emitter emitter;float position[3],direction[3];int status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memset(&emitter,0,sizeof(emitter));memcpy(emitter.position,wire,12);memcpy(emitter.direction,wire+3,12);emitter.room=wire[6];memcpy(position,wire+7,12);memcpy(direction,wire+10,12);move_room=wire[13];memcpy(&move_status,wire+14,4);memset(move_trace,0,sizeof(move_trace));
            status=rf_particle_emitter_move(&emitter,position,direction,move_locate,NULL);
            fwrite(&status,4,1,stdout);fwrite(emitter.position,12,1,stdout);fwrite(emitter.direction,12,1,stdout);fwrite(&emitter.room,4,1,stdout);fwrite(move_trace,36,1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-state")) {
        struct {uint32_t action;int32_t now;uint32_t count,uids[8],objects[4][3];} in;
        static rf_level_particle_state state;rf_level_particle_binding bindings[4];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_level_particles p={0};uint32_t i,out[9];int status;
            if(in.count>8)return 2;memset(&state,0,sizeof(state));p.state=&state;p.materials.bindings=bindings;p.materials.count=4;
            for(i=0;i<4;i++) {
                bindings[i].uid=state.slots[i].source_id=in.objects[i][0];state.slots[i].active=1;
                state.slots[i].runtime.enabled=in.objects[i][1];state.slots[i].runtime.emitter.deadline=(int32_t)in.objects[i][2];
            }
            status=rf_level_particles_set_state(&p,in.uids,in.count,in.action,in.now);out[0]=(uint32_t)status;
            for(i=0;i<4;i++){out[1+i*2]=state.slots[i].runtime.enabled;out[2+i*2]=(uint32_t)state.slots[i].runtime.emitter.deadline;}
            fwrite(out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-world-quad")) {
        struct {rf_visibility_camera camera;rf_particle_billboard_vertex vertices[4];} in;
        struct {int32_t status;rf_particle_screen_polygon polygon;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0,sizeof(out));out.status=rf_particle_world_quad(&in.camera,in.vertices,&out.polygon);
            fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--attachment-order")) {
        uint32_t in[48],i;rf_attachment_node *scratch[16];attachment_probe_context c;
        rf_attachment_backend backend={attachment_probe_lookup,attachment_probe_publish,&c};
        struct {int32_t status;uint32_t flags[16],count,events[96];} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){memset(&c,0,sizeof(c));memset(&out,0,sizeof(out));
            for(i=0;i<16;++i){c.flags[i]=in[i];c.nodes[i].flags=c.flags+i;c.nodes[i].parent=in[16+i];}
            for(i=0;i<16;++i){if(in[32+i]>=16){out.status=RF_RANGE;break;}
                out.status=rf_attachment_update(c.nodes+in[32+i],&backend,scratch,16);if(out.status)break;}
            memcpy(out.flags,c.flags,64);out.count=c.count;memcpy(out.events,c.events,sizeof(c.events));fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--attachment-local-pose")) {
        float in[33];struct {int32_t status;float pose[12];} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){memset(&out,0xa5,sizeof(out));
            out.status=rf_attachment_local_pose(in,in+12,in+15,in+24,out.pose);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-beam-project")) {
        struct {rf_visibility_camera camera;float end[3],start[3],width;} in;
        struct {int32_t status;rf_particle_screen_polygon polygon;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1){memset(&out,0,sizeof(out));
            out.status=rf_volume_beam_project(&in.camera,in.end,in.start,in.width,&out.polygon);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--volume-beam")) {
        float in[10];struct {int32_t status;rf_particle_billboard_vertex vertices[4];} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){memset(&out,0xa5,sizeof(out));
            out.status=rf_volume_beam_build(in,in+3,in+6,in[9],out.vertices);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--glare-volume-camera")) {
        float in[10];struct {int32_t status;float opacity;uint32_t draw;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){memset(&out,0xa5,sizeof(out));
            out.status=rf_glare_volume_camera_opacity(in,in+3,in+6,in[9],&out.opacity,&out.draw);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--glare-volume-opacity")) {
        uint32_t in[3];double angle;float cone;struct {int32_t status;float opacity;uint32_t draw;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){memcpy(&angle,in,8);memcpy(&cone,in+2,4);memset(&out,0xa5,sizeof(out));
            out.status=rf_glare_volume_opacity(angle,cone,&out.opacity,&out.draw);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--glare-parent")) {
        uint32_t in[5];rf_glare_base_owner owner;rf_object_registry registry;struct {int32_t status;uint32_t flags;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){uint32_t slot=in[1]&0xffff;memset(&owner,0xa5,sizeof(owner));memset(&registry,0,sizeof(registry));
            owner.flags=in[0];owner.parent_handle=in[1];owner.state.parent=in[4];
            if(slot<RF_OBJECT_CAPACITY){registry.slots[slot].handle=in[2];registry.slots[slot].object=in[3]?&owner:NULL;}
            out.status=rf_glare_parent_update(&owner,&registry);out.flags=owner.flags;fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--glare-tag-publish")) {
        struct {uint32_t flags;float radius,pose[12];} in;rf_glare_base_owner owner;
        struct {int32_t status;uint32_t flags;float positions[9],bounds[6],matrices[27];} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1){memset(&owner,0xa5,sizeof(owner));owner.flags=in.flags;owner.body.state.bounds.radius=in.radius;
            memset(&out,0,sizeof(out));out.status=rf_glare_publish_tag_pose(&owner,in.pose);out.flags=owner.flags;
            memcpy(out.positions,owner.position,12);memcpy(out.positions+3,owner.body.state.position,12);memcpy(out.positions+6,owner.body.state.next_position,12);
            memcpy(out.bounds,owner.body.state.bounds.minimum,24);memcpy(out.matrices,owner.matrix,36);
            memcpy(out.matrices+9,owner.body.state.orientation,36);memcpy(out.matrices+18,owner.body.state.next_orientation,36);
            fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--bitmap-animation-frame")) {
        uint32_t in[5];struct {int32_t status,frame;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1){out.frame=0x12345678;
            out.status=rf_bitmap_animation_frame(in[0],in[1],in[2],in[3],in[4],&out.frame);fwrite(&out,sizeof(out),1,stdout);}
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corona-oriented")) {
        float in[13];struct {int32_t status;uint32_t kind;rf_particle_billboard_vertex vertices[4];} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&out,0,sizeof(out));out.status=rf_corona_oriented_build(in,in+3,in+6,in+9,in[12],out.vertices,&out.kind);
            fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-world-stretch")) {
        struct {rf_visibility_camera camera;float position[3],previous[3],radius;uint32_t width,height;} in;
        struct {int32_t status;rf_particle_screen_polygon polygon;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0,sizeof(out));out.status=rf_particle_world_stretch(&in.camera,in.position,in.previous,in.radius,in.width,in.height,&out.polygon);
            fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-stretch")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        float in[10];struct {int32_t status;uint32_t fallback;rf_particle_billboard_vertex vertices[4];} out;
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&out,0,sizeof(out));out.status=rf_particle_stretch_build(in,in+3,in+6,in[9],out.vertices,&out.fallback);
            fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--level-particle-queue")) {
        struct {rf_visibility_frustum frustum;uint32_t room,count;rf_particle particles[8];
            rf_emitter_slot emitters[4];rf_level_particle_object parent;} in;
        static rf_level_particle_state state;static rf_render_queue_record records[2048];
        const uint32_t next[4]={3,129,0,1};
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_level_particles particles={0};uint32_t i;int32_t status;
            memset(&state,0,sizeof(state));particles.state=&state;
            rf_particle_pool_init(&state.particles,state.records,state.lists,133);
            rf_emitter_pool_init(&state.emitters,state.slots,&state.particles);
            memcpy(state.records,in.particles,sizeof(in.particles));memcpy(state.slots,in.emitters,sizeof(in.emitters));
            for(i=0;i<8;i++)state.records[i].next=i+3<8?i+3:1600+(i%3==0?2:i%3==1?4:3);
            state.lists[2].next=0;state.lists[4].next=1;state.lists[3].next=2;
            state.emitters.lists[1].next=2;for(i=0;i<4;i++)state.slots[i].next=next[i];
            memset(records,0xa5,sizeof(records));
            status=rf_level_particles_queue_room(&particles,in.room,&in.frustum,queue_parent_lookup,&in.parent,records,2048,&in.count);
            fwrite(&status,4,1,stdout);fwrite(&in.count,4,1,stdout);fwrite(records,sizeof(records),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-queue-append")) {
        struct {rf_visibility_frustum frustum;rf_render_queue_record entry;uint32_t count;} in;
        rf_render_queue_record records[2048];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            int32_t status;uint32_t accepted=0;
            memset(records,0xa5,sizeof(records));
            status=rf_render_queue_append(&in.frustum,in.entry.position,&in.entry,records,2048,&in.count,&accepted);
            fwrite(&status,4,1,stdout);fwrite(&accepted,4,1,stdout);fwrite(&in.count,4,1,stdout);
            fwrite(records,sizeof(records),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-room-order")) {
        struct {uint32_t count;float camera[3];rf_render_room_split split;} in;
        rf_render_room_entry entries[2048];uint32_t order[2048],scratch[6144];float distances[2048];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            int32_t status;uint32_t before;
            if(in.count>2048 || fread(entries,sizeof(*entries),in.count,stdin)!=in.count)return 2;
            status=rf_render_room_order(entries,in.count,in.camera,&in.split,order,distances,scratch,&before);
            fwrite(&status,4,1,stdout);if(status)continue;
            fwrite(&before,4,1,stdout);fwrite(order,4,in.count,stdout);fwrite(distances,4,in.count,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-group-order")) {
        struct {uint32_t count;float camera[3];} in;
        rf_render_group_entry entries[2048];uint32_t order[2048],scratch[4096];float distances[2048];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            int32_t status;
            if(in.count>2048 || fread(entries,sizeof(*entries),in.count,stdin)!=in.count)return 2;
            status=rf_render_group_order(entries,in.count,in.camera,order,distances,scratch);
            fwrite(&status,4,1,stdout);if(status)continue;
            fwrite(order,4,in.count,stdout);fwrite(distances,4,in.count,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-sphere-order")) {
        struct {uint32_t count;float camera[3];} in;
        rf_render_sphere entries[2048];uint32_t order[2048];float distances[2048];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            int32_t status;
            if(in.count>2048 || fread(entries,sizeof(*entries),in.count,stdin)!=in.count)return 2;
            status=rf_render_sphere_order(entries,in.count,in.camera,order,distances);
            fwrite(&status,4,1,stdout);if(status)continue;
            fwrite(order,4,in.count,stdout);fwrite(distances,4,in.count,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-world-billboard")) {
        struct {rf_visibility_camera camera;float position[3],angle,radius;uint32_t width,height;} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_particle_screen_polygon out={0};int32_t status=rf_particle_world_billboard(&in.camera,
                in.position,in.angle,in.radius,in.width,in.height,&out);
            fwrite(&status,4,1,stdout);fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--visibility-projected")) {
        struct {rf_visibility_camera_parameters camera;uint32_t start,special,flags;float rect[4];
            rf_visibility_room_links rooms[4];uint32_t links[10];rf_visibility_portal portals[5];
            rf_visibility_portal_cache cache[5];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_visibility_camera camera={0};rf_visibility_frame scratch[257];uint32_t stage,i;
            rf_visibility_portal_view view={&camera,in.cache,in.portals,5,1920,1080};
            for(stage=0;stage<3;stage++) {
                rf_room_visibility rooms[4]={0};uint32_t order[4]={0};rf_visibility state={rooms,order,4,0};int32_t status;
                if(stage==1)in.camera.origin[0]+=8;
                status=rf_visibility_camera_setup(&in.camera,&camera);if(status)return 2;
                rf_visibility_begin_view(&state);
                if(stage!=1)rf_visibility_portals_begin_view(in.cache,5);
                status=rf_visibility_traverse_projected(&state,in.rooms,in.links,10,&view,in.start,in.special,in.flags,in.rect,scratch);
                fwrite(&status,4,1,stdout);fwrite(&state.visible_count,4,1,stdout);
                fwrite(rooms,sizeof(rooms),1,stdout);fwrite(order,sizeof(order),1,stdout);
                fwrite(in.portals,sizeof(in.portals),1,stdout);
                for(i=0;i<5;i++)fwrite(&in.cache[i].valid,4,1,stdout);
            }
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--visibility-camera")) {
        rf_visibility_camera_parameters in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_visibility_camera out;int32_t status;memset(&out,0xa5,sizeof(out));
            status=rf_visibility_camera_setup(&in,&out);
            fwrite(&status,4,1,stdout);fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--visibility-view-scale")) {
        rf_visibility_viewport in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_visibility_view_scale out;int32_t status;memset(&out,0xa5,sizeof(out));
            status=rf_visibility_view_scale_build(&in,&out);
            fwrite(&status,4,1,stdout);fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--visibility-frustum")) {
        rf_visibility_view in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_visibility_frustum out;int32_t status;memset(&out,0xa5,sizeof(out));
            status=rf_visibility_frustum_build(&in,&out);
            fwrite(&status,4,1,stdout);fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--visibility-plane")) {
        struct {uint32_t mode;float a[3],b[3],c[3];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_visibility_plane plane;int32_t status;memset(&plane,0xa5,sizeof(plane));
            status=in.mode?rf_visibility_plane_points(in.a,in.b,in.c,&plane):rf_visibility_plane_normal(in.a,in.b,&plane);
            fwrite(&status,4,1,stdout);fwrite(&plane,sizeof(plane),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--box-project")) {
        struct {rf_visibility_projection view;float minimum[3],maximum[3];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_visibility_screen_bounds out={0,{11,22,33,44}};int32_t status;
            status=rf_visibility_box_project(&in.view,in.minimum,in.maximum,&out);
            fwrite(&status,4,1,stdout);fwrite(&out,sizeof(out),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--portal-classify")) {
        struct {float camera[3],minimum[3],maximum[3];uint32_t count;rf_visibility_plane planes[8];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            uint32_t action=0xa5a5a5a5;int32_t status;
            if(in.count>8)return 2;
            status=rf_visibility_portal_classify(in.camera,in.minimum,in.maximum,in.planes,in.count,&action);
            fwrite(&status,4,1,stdout);fwrite(&action,4,1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--visibility-traverse")) {
        struct {uint32_t start,special,flags;float rect[4];rf_visibility_room_links rooms[4];
            uint32_t links[10];rf_visibility_portal portals[5];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_room_visibility rooms[4]={0};uint32_t order[4]={0};rf_visibility state={rooms,order,4,0};
            rf_visibility_frame scratch[257];int32_t status;
            rf_visibility_begin_view(&state);
            status=rf_visibility_traverse(&state,in.rooms,in.links,10,in.portals,5,in.start,in.special,in.flags,in.rect,scratch);
            fwrite(&status,4,1,stdout);fwrite(&state.visible_count,4,1,stdout);
            fwrite(rooms,sizeof(rooms),1,stdout);fwrite(order,sizeof(order),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    if(argc==2 && !strcmp(argv[1],"--visibility")) {
        struct {uint32_t op,index,depth;float rectangle[4];} in;
        rf_room_visibility rooms[8]={0};uint32_t order[8]={0};rf_visibility state={rooms,order,8,0};
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            int32_t status=in.op==0?rf_visibility_begin_render(&state):in.op==1?rf_visibility_begin_view(&state):
                rf_visibility_visit(&state,in.index,in.rectangle,in.depth);
            fwrite(&status,4,1,stdout);fwrite(&state.visible_count,4,1,stdout);
            fwrite(rooms,sizeof(rooms),1,stdout);fwrite(order,sizeof(order),1,stdout);
        }
        return ferror(stdin)||ferror(stdout);
    }
    struct { int32_t index; uint32_t override_mode; int32_t enabled,now;
        rf_effect_switch objects[4]; int32_t slots[4]; } input;
    rf_effect_pair pair; unsigned i; int32_t status;
    _Static_assert(sizeof(input)==64,"Effect fixture layout");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--level-emitter-template")) {
        struct {rf_level_emitter level;rf_particle_emitter_template initial;} in;
        struct {int32_t status;rf_particle_emitter_template result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.result=in.initial;out.status=rf_level_emitter_template(&in.level,23,1,&out.result);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--emitter-pool")) {
        struct {uint32_t operation,index,now;rf_particle_emitter_template source;} in;
        struct {int32_t status;uint32_t index,seed,live;rf_emitter_slot slot;rf_particle particle;
            uint32_t links[128][2];rf_particle_list heads[2];uint32_t particle_live[2];} out;
        static rf_particle records[1600];static rf_emitter_slot slots[128];rf_particle_list lists[133];
        rf_particle_pool particles;rf_emitter_pool pool;rf_random_state rng={123};
        rf_particle_pool_init(&particles,records,lists,133);rf_emitter_pool_init(&pool,slots,&particles);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            uint32_t selected=UINT32_MAX,particle=UINT32_MAX;
            memset(&out,0,sizeof(out));out.index=UINT32_MAX;
            if(in.operation==0) {
                out.status=rf_emitter_pool_create(&pool,&in.source,-1,0x2468,0,(int32_t)in.now,NULL,&rng,&out.index);
                if(!out.status){selected=out.index;particle=lists[selected+5].next;}
            } else if(in.operation==1 && in.index<128) {
                selected=in.index;particle=lists[selected+5].next;
                slots[selected].runtime.emitter.spawn.copied_48=0x11223344;
                slots[selected].bounds.center[0]=1.25f;slots[selected].bounds.center[1]=-2.5f;slots[selected].bounds.center[2]=7;
                slots[selected].runtime.duration=0.75f;slots[selected].runtime.elapsed=0.125f;
                out.status=rf_emitter_pool_release(&pool,selected);
            } else out.status=RF_RANGE;
            out.seed=rng.value;out.live=pool.live;
            if(selected<128)out.slot=slots[selected];if(particle<1600)out.particle=records[particle];
            for(i=0;i<128;i++){out.links[i][0]=slots[i].next;out.links[i][1]=slots[i].previous;}
            memcpy(out.heads,pool.lists,sizeof(out.heads));memcpy(out.particle_live,particles.live,sizeof(out.particle_live));
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-emitter-fresh")) {
        rf_particle_emitter_runtime in;
        struct {int32_t status;rf_particle_emitter_runtime runtime;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.runtime=in;out.status=rf_particle_emitter_fresh(&out.runtime);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-emitter-init")) {
        struct {rf_particle_emitter_runtime runtime;rf_particle_emitter_template source;uint32_t seed,now,empty,enabled;} in;
        struct {int32_t status;uint32_t seed;rf_particle_emitter_runtime runtime;
            rf_particle_emitter_init_result result;rf_particle particle;} out;
        static rf_particle particles[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        _Static_assert(sizeof(rf_particle_emitter_template)==132,"Emitter template layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};rf_particle_pool_init(&pool,particles,lists,6);
            if(in.empty)lists[1].next=lists[1].previous=RF_PARTICLE_CAPACITY+1;
            memset(&particles[500],0xa5,sizeof(particles[500]));
            particles[500].next=501;particles[500].previous=RF_PARTICLE_CAPACITY+1;
            memset(&out,0xa5,sizeof(out));out.runtime=in.runtime;
            out.status=rf_particle_emitter_initialize(&pool,&out.runtime,&in.source,-1,0x2468,1,in.enabled,
                (int32_t)in.now,NULL,&rng,&out.result);
            out.seed=rng.value;out.particle=particles[500];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-update")) {
        struct {rf_particle_emitter_runtime runtime;uint32_t seed,now,empty;
            rf_particle_emitter_parent parent;uint32_t present,global_enabled;float dt;uint32_t room;} in;
        struct {int32_t status;uint32_t seed;rf_particle_emitter_runtime runtime;
            rf_particle_emitter_update_result result;rf_particle particle;} out;
        static rf_particle particles[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};rf_particle_pool_init(&pool,particles,lists,6);
            if(in.empty)lists[1].next=lists[1].previous=RF_PARTICLE_CAPACITY+1;
            memset(&particles[500],0xa5,sizeof(particles[500]));
            particles[500].next=501;particles[500].previous=RF_PARTICLE_CAPACITY+1;
            memset(&out,0xa5,sizeof(out));out.runtime=in.runtime;
            out.status=rf_particle_emitter_update(&pool,&out.runtime,1,in.global_enabled,in.dt,(int32_t)in.now,
                in.present?&in.parent:NULL,in.room,&rng,&out.result);
            out.seed=rng.value;out.particle=particles[500];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-emission-parent")) {
        struct {rf_particle_emitter emitter;uint32_t seed,now,empty;rf_particle_emitter_parent parent;uint32_t present;} in;
        struct {int32_t status;uint32_t seed,index;rf_particle_emitter emitter;rf_particle particle;} out;
        static rf_particle particles[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};rf_particle_pool_init(&pool,particles,lists,6);
            if(in.empty)lists[1].next=lists[1].previous=RF_PARTICLE_CAPACITY+1;
            memset(&particles[500],0xa5,sizeof(particles[500]));
            particles[500].next=501;particles[500].previous=RF_PARTICLE_CAPACITY+1;
            memset(&out,0xa5,sizeof(out));out.emitter=in.emitter;
            out.status=rf_particle_emitter_emit_parent(&pool,&out.emitter,1,(int32_t)in.now,in.present?&in.parent:NULL,&rng,&out.index);
            out.seed=rng.value;out.particle=particles[500];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-emission")) {
        struct {rf_particle_emitter emitter;uint32_t seed,now,empty;} in;
        struct {int32_t status;uint32_t seed,index;rf_particle_emitter emitter;rf_particle particle;} out;
        static rf_particle particles[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};rf_particle_pool_init(&pool,particles,lists,6);
            if(in.empty)lists[1].next=lists[1].previous=RF_PARTICLE_CAPACITY+1;
            memset(&particles[500],0xa5,sizeof(particles[500]));
            particles[500].next=501;particles[500].previous=RF_PARTICLE_CAPACITY+1;
            memset(&out,0xa5,sizeof(out));out.emitter=in.emitter;
            out.status=rf_particle_emitter_emit(&pool,&out.emitter,1,(int32_t)in.now,&rng,&out.index);
            out.seed=rng.value;out.particle=particles[500];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-render-mode")) {
        uint32_t in[3],out;
        while(fread(in,sizeof(in),1,stdin)==1) {
            out=rf_particle_render_mode(in[0],in[1],in[2]);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-texture-states")) {
        uint32_t in[2];struct {int32_t status;rf_particle_texture_states states;} out;
        _Static_assert(sizeof(out)==152,"Particle texture-state output");
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_texture_decode(in[0],in[1],&out.states);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-render-states")) {
        struct {uint32_t mode;rf_particle_render_environment environment;rf_particle_render_states states;} in;
        struct {int32_t status;rf_particle_render_states states;} out;
        _Static_assert(sizeof(in)==116,"Particle render-state input");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.states=in.states;out.status=rf_particle_render_decode(in.mode,&in.environment,&out.states);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-cone-oriented")) {
        struct {float axis[3],cosine_min;rf_random_state random;} in;
        struct {int32_t status;rf_random_state random;float direction[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;
            out.status=rf_particle_cone_oriented(in.axis,in.cosine_min,&out.random,out.direction);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-cone")) {
        struct {float cosine_min;rf_random_state random;} in;
        struct {int32_t status;rf_random_state random;float direction[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;
            out.status=rf_particle_cone_sample(in.cosine_min,&out.random,out.direction);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-vertex-encode")) {
        struct {rf_particle_vertex_environment environment;rf_particle_screen_vertex vertex;} in;
        struct {int32_t status;rf_particle_draw_vertex vertex;} out;
        _Static_assert(sizeof(in)==80 && sizeof(out)==36,"Particle draw vertex fixture layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_vertex_encode(&in.environment,&in.vertex,&out.vertex);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard-project")) {
        struct {rf_particle_projection projection;rf_particle_clip_environment environment;rf_particle_billboard_packet packet;} in;
        struct {int32_t status;rf_particle_screen_polygon polygon;} out;
        _Static_assert(sizeof(in)==148 && sizeof(out)==392,"Particle screen fixture layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_project(&in.projection,&in.environment,&in.packet,&out.polygon);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard-clip")) {
        struct {rf_particle_clip_environment environment;rf_particle_billboard_packet packet;} in;
        struct {int32_t status;rf_particle_clipped_polygon polygon;} out;
        _Static_assert(sizeof(in)==124 && sizeof(out)==308,"Clipped particle fixture layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_clip(&in.environment,&in.packet,&out.polygon);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard-prepare")) {
        struct {float center[3],angle,radius;uint32_t width,height;float scale[3];rf_particle_clip_environment clip;} in;
        struct {int32_t status;rf_particle_billboard_packet packet;} out;
        _Static_assert(sizeof(in)==56 && sizeof(out)==112,"Particle prepared billboard fixtures");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_prepare(in.center,in.angle,in.radius,in.width,in.height,in.scale,&in.clip,&out.packet);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-project")) {
        struct {rf_particle_projection projection;rf_particle_projected_point point;} in;
        struct {int32_t status;rf_particle_projected_point point;} out;
        _Static_assert(sizeof(in)==52,"Particle projection input");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.point=in.point;out.status=rf_particle_project(&in.projection,&out.point);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard")) {
        struct {float center[3],angle,radius;uint32_t width,height;float scale[2];} in;
        struct {int32_t status;rf_particle_billboard_vertex vertices[4];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_build(in.center,in.angle,in.radius,in.width,in.height,in.scale,out.vertices);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-frame")) {
        rf_particle in;struct {int32_t status;uint32_t frame;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.frame=0xa5a5a5a5;out.status=rf_particle_frame_index(&in,&out.frame);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--emitter-finish-bounds")) {
        struct {uint32_t enabled;int32_t owner;float maximum,radius,previous;} in;
        struct {int32_t status;float maximum,radius;} out;
        rf_emitter_slot slots[1];rf_emitter_pool pool={0};pool.slots=slots;pool.lists[1].next=0;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(slots,0,sizeof(slots));slots[0].next=129;
            slots[0].bounds.owner=in.owner;slots[0].bounds.maximum_distance_squared=in.maximum;
            slots[0].runtime.emitter.max_radius=in.radius;slots[0].estimated_radius=in.previous;
            out.status=rf_emitter_pool_finish_bounds(&pool,in.enabled);
            out.maximum=slots[0].bounds.maximum_distance_squared;out.radius=slots[0].estimated_radius;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-step-resolved")) {
        struct {float dt;rf_particle particle;rf_particle_emitter_bounds bounds;rf_particle_owner_gate gate;} in;
        struct {int32_t status;uint32_t live;rf_particle particle;rf_particle_emitter_bounds bounds;} out;
        static rf_particle records[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        rf_particle_pool_init(&pool,records,lists,6);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            records[0]=in.particle;pool.live[0]=1;pool.live[1]=0;
            lists[0].next=lists[0].previous=1600;lists[5].next=lists[5].previous=0;
            out.status=rf_particle_pool_step_resolved(&pool,0,in.dt,&in.bounds,&in.gate);out.live=pool.live[0];out.particle=records[0];out.bounds=in.bounds;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-step-unowned")) {
        struct {float dt;rf_particle particle;rf_particle_emitter_bounds bounds;} in;
        struct {int32_t status;uint32_t live;rf_particle particle;rf_particle_emitter_bounds bounds;} out;
        static rf_particle records[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        rf_particle_pool_init(&pool,records,lists,6);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            records[0]=in.particle;pool.live[0]=1;pool.live[1]=0;
            lists[0].next=lists[0].previous=1600;lists[5].next=lists[5].previous=0;
            out.status=rf_particle_pool_step_unowned(&pool,0,in.dt,&in.bounds);out.live=pool.live[0];out.particle=records[0];out.bounds=in.bounds;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-step-free")) {
        struct {float dt;rf_particle particle;} in;
        struct {int32_t status;uint32_t live;rf_particle particle;} out;
        static rf_particle records[RF_PARTICLE_CAPACITY];rf_particle_list lists[5];rf_particle_pool pool;
        rf_particle_pool_init(&pool,records,lists,5);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            records[0]=in.particle;pool.live[0]=1;pool.live[1]=0;
            lists[0].next=lists[0].previous=1600;lists[2].next=lists[2].previous=0;
            out.status=rf_particle_pool_step_free(&pool,0,in.dt);out.live=pool.live[0];out.particle=records[0];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-pool")) {
        static rf_particle records[RF_PARTICLE_CAPACITY];rf_particle_list lists[8];rf_particle_pool pool;
        struct {uint32_t operation,kind,owner,room,emitter,index,seed;rf_particle_spawn spawn;} in;
        struct {int32_t status;uint32_t index,seed,live[2];} out;
        _Static_assert(sizeof(in)==104,"Pool command layout");
        rf_particle_pool_init(&pool,records,lists,8);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};out.index=in.index;
            if(in.operation==0)out.status=rf_particle_pool_create(&pool,in.kind,&in.spawn,in.owner,in.room,in.emitter,&rng,&out.index);
            else if(in.operation==1)out.status=rf_particle_pool_detach(&pool,in.emitter);
            else if(in.operation==2)out.status=rf_particle_pool_recycle(&pool,in.index);
            else out.status=in.operation==3?RF_OK:RF_RANGE;
            out.seed=rng.value;out.live[0]=pool.live[0];out.live[1]=pool.live[1];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
            if(in.operation==3 && (fwrite(records,sizeof(records),1,stdout)!=1 || fwrite(lists,sizeof(lists),1,stdout)!=1))return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-initialize")) {
        struct {rf_particle_spawn spawn;uint32_t pool,owner,room,emitter;rf_random_state random;rf_particle particle;} in;
        struct {int32_t status;rf_random_state random;rf_particle particle;} out;
        _Static_assert(sizeof(rf_particle_spawn)==76,"Particle spawn layout");
        _Static_assert(sizeof(rf_particle)==120,"Particle record layout");
        _Static_assert(sizeof(in)==216,"Particle initialization fixture");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.random=in.random;out.particle=in.particle;
            out.status=rf_particle_initialize(&in.spawn,in.pool,in.owner,in.room,in.emitter,&out.random,&out.particle);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-tick")) {
        struct {rf_particle_cycle cycle;uint32_t global_enabled;float dt;uint32_t due;rf_random_state random;rf_particle_emitter_clock clock;} in;
        struct {int32_t status;rf_random_state random;rf_particle_emitter_clock clock;rf_particle_emitter_actions actions;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;out.clock=in.clock;
            out.status=rf_particle_emitter_tick(&in.cycle,in.global_enabled,in.dt,in.due,&out.random,&out.clock,&out.actions);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-duration")) {
        struct {rf_particle_cycle cycle;unsigned enabled;rf_random_state random;} in;
        struct {int32_t status;rf_random_state random;float duration;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;
            out.status=rf_particle_cycle_duration(&in.cycle,in.enabled,&out.random,&out.duration);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--explosion-clock")) {
        struct {rf_explosion_recipe recipe;float dt;rf_explosion_clock clock;} in;
        struct {int32_t status;rf_explosion_clock clock;rf_explosion_clock_actions actions;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.clock=in.clock;
            out.status=rf_explosion_clock_tick(&in.recipe,in.dt,&out.clock,&out.actions);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--explosion-scale")) {
        struct {rf_explosion_definition definition;uint32_t slot;float size;} in;
        struct {int32_t status;rf_particle_definition particle;float extent;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_explosion_central_prepare(&in.definition,in.slot,in.size,&out.particle,&out.extent);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==5 && !strcmp(argv[1],"--explosion-definition")) {
        rf_vpp archive;rf_explosion_definition definition;
        _Static_assert(sizeof(definition)==2380,"Explosion definition fixture layout");
        memset(&definition,0xa5,sizeof(definition));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_explosion_definition_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&definition);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&definition,sizeof(definition),1,stdout)==1?0:1;
    }
    if(argc==5 && !strcmp(argv[1],"--explosion-load")) {
        rf_vpp archive;rf_explosion_recipe recipe;
        _Static_assert(sizeof(recipe)==712,"Explosion recipe fixture layout");
        memset(&recipe,0xa5,sizeof(recipe));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_explosion_recipe_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&recipe);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&recipe,sizeof(recipe),1,stdout)==1?0:1;
    }
    if(argc==5 && !strcmp(argv[1],"--vclip-load")) {
        rf_vpp archive;rf_vclip_definition definition;
        _Static_assert(sizeof(definition)==500,"Vclip fixture layout");
        memset(&definition,0xa5,sizeof(definition));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_vclip_definition_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&definition);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&definition,sizeof(definition),1,stdout)==1?0:1;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-prepare")) {
        rf_particle_definition definition;
        while(fread(&definition,sizeof(definition),1,stdin)==1) {
            if(rf_particle_definition_prepare(&definition,&definition))return 3;
            if(fwrite(&definition,sizeof(definition),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==5 && !strcmp(argv[1],"--emitter-load")) {
        rf_vpp archive;rf_particle_definition definition;
        memset(&definition,0xa5,sizeof(definition));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_emitter_definition_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&definition);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&definition,sizeof(definition),1,stdout)==1?0:1;
    }
    if(argc==2 && (!strcmp(argv[1],"--particle-definition") || !strcmp(argv[1],"--emitter-definition"))) {
        uint32_t bytes;rf_particle_definition definition;
        int named=!strcmp(argv[1],"--emitter-definition");char name[128];
        _Static_assert(sizeof(definition)==184,"Particle definition fixture layout");
        while(fread(&bytes,4,1,stdin)==1) {
            if(named && (fread(name,1,128,stdin)!=128 || !memchr(name,0,128)))return 2;
            void *text;if(bytes>65536)return 2;text=malloc(bytes?bytes:1);if(!text)return 2;
            if(fread(text,1,bytes,stdin)!=bytes){free(text);return 2;}
            memset(&definition,0xa5,sizeof(definition));
            status=named?rf_emitter_definition_read(text,bytes,name,&definition):rf_particle_definition_read(text,bytes,&definition);free(text);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&definition,sizeof(definition),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-cycle")) {
        struct {rf_particle_text_flags flags;unsigned initially_on,alternate;rf_particle_cycle cycle;} c;
        _Static_assert(sizeof(c)==36,"Particle cycle fixture layout");
        while(fread(&c,sizeof(c),1,stdin)==1) {
            if(rf_particle_cycle_read(&c.flags,c.initially_on,c.alternate,&c.cycle,&c.cycle))return 3;
            if(fwrite(&c.flags,sizeof(c.flags),1,stdout)!=1 || fwrite(&c.cycle,sizeof(c.cycle),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-pack")) {
        struct {rf_particle_text_flags flags;unsigned present;int values[4];} pack;
        _Static_assert(sizeof(pack)==32,"Particle packing fixture layout");
        while(fread(&pack,sizeof(pack),1,stdin)==1) {
            if(rf_particle_flags_pack(&pack.flags,pack.present,pack.values))return 3;
            if(fwrite(&pack.flags,sizeof(pack.flags),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-flags")) {
        char strings[2][512];rf_particle_text_flags flags;
        while(fread(strings,sizeof(strings),1,stdin)==1) {
            if(!memchr(strings[0],0,512) || !memchr(strings[1],0,512))return 2;
            if(rf_particle_flags_read(strings[0],strings[1],&flags))return 3;
            if(fwrite(&flags,sizeof(flags),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--vclip-lookup")) {
        char strings[65][128];const char *names[64];
        while(fread(strings,sizeof(strings),1,stdin)==1) {
            for(i=0;i<65;++i)if(!memchr(strings[i],0,128))return 2;
            for(i=0;i<64;++i)names[i]=strings[i];
            status=rf_vclip_name_lookup(names,strings[64]);
            if(fwrite(&status,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    while (fread(&input,sizeof(input),1,stdin)==1) {
        for (i=0;i<4;++i) pair.objects[i/2][i%2]=input.slots[i]>=0 && input.slots[i]<4 ? &input.objects[input.slots[i]] : NULL;
        status=rf_effect_set_enabled(&pair,1,input.index,input.override_mode,input.enabled,input.now);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(input.objects,sizeof(input.objects),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
