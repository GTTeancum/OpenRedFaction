#include "rf/lightmap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static uint32_t u32(const unsigned char *p)
{ return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
void rf_lightmaps_close(rf_lightmaps *maps)
{
    uint32_t i;
    if (!maps) return;
    for (i = 0; i < maps->count; ++i) rf_image_close(maps->images+i);
    free(maps->images); memset(maps, 0, sizeof(*maps));
}
int rf_lightmaps_open(rf_lightmaps *maps, const rf_level *level, uint32_t budget)
{
    const rf_level_section *section;
    unsigned char header[8], rgb[1536], packed[1024];
    uint32_t count, at = 4, i;
    uint64_t records;
    int result;
    if (!maps) return RF_RANGE;
    memset(maps, 0, sizeof(*maps));
    if (!level || level->version != 180) return RF_FORMAT;
    section = rf_level_find(level, 0x1200);
    if (!section) return RF_NOT_FOUND;
    if (section->size < 4) return RF_FORMAT;
    result = rf_level_read(level, section, 0, header, 4);
    if (result) return result;
    count = u32(header);
    if ((uint64_t)count * 11 > section->size - 4) return RF_FORMAT;
    records = (uint64_t)count * sizeof(rf_image);
    if (records > budget) return RF_RANGE;
    if (count) {
        maps->images = (rf_image *)calloc(count, sizeof(rf_image));
        if (!maps->images) return RF_RANGE;
    }
    maps->count = count; maps->allocated_bytes = (uint32_t)records;
    for (i = 0; i < count; ++i) {
        rf_image *image = maps->images+i;
        uint32_t pixels, decoded = 0;
        result = RF_FORMAT;
        if (section->size - at < 8) goto fail;
        result = rf_level_read(level, section, at, header, 8); at += 8;
        if (result) goto fail;
        image->width = u32(header); image->height = u32(header+4);
        result = RF_FORMAT;
        if (!image->width || !image->height || image->width > 4096 || image->height > 4096) goto fail;
        pixels = image->width * image->height;
        if ((uint64_t)pixels*3 > section->size-at) goto fail;
        result = RF_RANGE;
        image->bytes = pixels*2; image->source_format=5;
        if (image->bytes > budget - maps->allocated_bytes) goto fail;
        result=rf_image_allocate_pixels(image);if(result)goto fail;
        maps->allocated_bytes += image->bytes;
        while (decoded < pixels) {
            uint32_t n = pixels-decoded, j;
            if (n > 512) n = 512;
            result = rf_level_read(level, section, at, rgb, n*3);
            if (result) goto fail;
            /* Both current world backends implement two-texture MODULATE2X.
             * Original mode102 with those capabilities skips RGB brightening. */
            result=rf_lightmap_pack_1555(rgb,n*3,n,1,rf_lightmap_requires_brightening(102,1,1),packed,n*2,n*2);
            if(result)goto fail;
            for (j = 0; j < n; ++j) {
                unsigned char *pixel=rf_image_pixel(image,(decoded+j)%image->width,(decoded+j)/image->width);
                memcpy(pixel,packed+j*2,2);
            }
            at += n*3; decoded += n;
        }
    }
    if (at != section->size) { result = RF_FORMAT; goto fail; }
    return RF_OK;
fail:
    rf_lightmaps_close(maps);
    return result;
}

/* Exact positive binary32 product/truncation, as the x87 caller before ftol.
 * Integer arithmetic avoids a host-double rounding crossing a texel boundary. */
static uint32_t lightmap_texel_index(uint32_t extent,float coordinate)
{
    uint32_t bits,exponent,shift;uint64_t product;
    memcpy(&bits,&coordinate,4);exponent=(bits>>23)&255u;
    if(!exponent)return 0;
    shift=150u-exponent;product=(uint64_t)extent*((bits&0x7fffffu)|0x800000u);
    return shift>=64?0:(uint32_t)(product>>shift);
}
int rf_lightmap_sample_1555(const rf_lightmap_1555_view *view,const float uv[2],uint32_t *color)
{
    uint64_t x,y,offset;uint32_t pixel,value;
    if(!view || !uv || !color)return RF_RANGE;
    if(!isfinite(uv[0]) || !isfinite(uv[1]) || uv[0]<0 || uv[0]>1 || uv[1]<0 || uv[1]>1)return RF_RANGE;
    if(!view->pixels){*color=0xffffffffu;return RF_OK;}
    if(!view->width || !view->height || view->width>INT32_MAX || view->height>INT32_MAX ||
       view->pitch>INT32_MAX || (uint64_t)view->width*2>view->pitch)return RF_RANGE;
    x=lightmap_texel_index(view->width,uv[0]);y=lightmap_texel_index(view->height,uv[1]);
    offset=y*view->pitch+x*2;
    if(offset+2>view->bytes)return RF_RANGE;
    pixel=view->pixels[offset]|(uint32_t)view->pixels[offset+1]<<8;
    value=((pixel>>7)&0xf8u)|(((pixel>>2)&0xf8u)<<8)|((pixel&31u)<<19)|0xff000000u;
    *color=value;return RF_OK;
}

int rf_lightmap_sample_image_1555(const rf_image *image,const float uv[2],uint32_t *color)
{
    uint64_t index;const unsigned char *p;uint32_t pixel;
    if(!image || !uv || !color)return RF_RANGE;
    if(!isfinite(uv[0]) || !isfinite(uv[1]) || uv[0]<0 || uv[0]>1 || uv[1]<0 || uv[1]>1)return RF_RANGE;
    if(!image->rgba){*color=0xffffffffu;return RF_OK;}
    if(!rf_image_is_packed_1555(image) || image->width>4096 || image->height>4096)return RF_RANGE;
    index=(uint64_t)lightmap_texel_index(image->height,uv[1])*image->width+lightmap_texel_index(image->width,uv[0]);
    if(index>=(uint64_t)image->width*image->height)return RF_RANGE;
    p=rf_image_pixel(image,(uint32_t)(index%image->width),(uint32_t)(index/image->width));
    pixel=p[0]|(uint32_t)p[1]<<8;
    *color=((pixel>>7)&0xf8u)|(((pixel>>2)&0xf8u)<<8)|((pixel&31u)<<19)|0xff000000u;return RF_OK;
}

int rf_lightmap_project(const rf_lightmap_projection *projection,const float point[3],float uv[2])
{
    float value[2];uint32_t i;
    if(!projection || !point || !uv)return RF_RANGE;
    for(i=0;i<2;++i)if(projection->axes[i]>2 || !isfinite(projection->scale[i]) || !isfinite(projection->offset[i]))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_RANGE;
    for(i=0;i<2;++i) {
        volatile float product=(float)((double)point[projection->axes[i]]*projection->scale[i]);
        value[i]=(float)((double)product+projection->offset[i]);
        if(value[i]<0)value[i]=0;else if(value[i]>1)value[i]=1;
    }
    memcpy(uv,value,sizeof(value));return RF_OK;
}

int rf_lightmap_projection_read(const void *record,uint32_t bytes,rf_lightmap_projection *projection)
{
    const unsigned char *p=record;rf_lightmap_projection value;uint32_t words[4],i;
    if(!record || !projection || bytes!=96)return RF_RANGE;
    value.axes[0]=u32(p+68);value.axes[1]=u32(p+72);
    words[0]=u32(p+84);words[1]=u32(p+88);words[2]=u32(p+76);words[3]=u32(p+80);
    memcpy(value.scale,words,8);memcpy(value.offset,words+2,8);
    for(i=0;i<2;++i)if(value.axes[i]>2 || !isfinite(value.scale[i]) || !isfinite(value.offset[i]))return RF_FORMAT;
    *projection=value;return RF_OK;
}

int rf_lightmap_pack_1555(unsigned char *rgb,uint32_t rgb_bytes,uint32_t width,uint32_t height,
    uint32_t double_rgb,unsigned char *packed,uint32_t pitch,uint32_t packed_bytes)
{
    uint64_t count=(uint64_t)width*height,i;uint32_t y,x,c[3],j,value;
    if(!rgb || !width || !height || double_rgb>1 || count>rgb_bytes/3u)return RF_RANGE;
    if(packed && ((pitch&1u) || (uint64_t)width*2>pitch ||
        (uint64_t)(height-1)*pitch+(uint64_t)width*2>packed_bytes))return RF_RANGE;
    if(double_rgb)for(i=0;i<count*3;++i){value=(uint32_t)rgb[i]*2+1;rgb[i]=(unsigned char)(value>255?255:value);}
    if(!packed)return RF_OK;
    for(y=0;y<height;++y)for(x=0;x<width;++x) {
        const unsigned char *source=rgb+((uint64_t)y*width+x)*3;
        unsigned char *destination=packed+(uint64_t)y*pitch+x*2;
        for(j=0;j<3;++j){c[j]=source[j]>>3;if(c[j]<4)c[j]=4;}
        value=0x8000u|(c[0]<<10)|(c[1]<<5)|c[2];destination[0]=(unsigned char)value;destination[1]=(unsigned char)(value>>8);
    }
    return RF_OK;
}

int rf_lightmap_edge_crossing(const float a[2],const float b[2],const float c[2],const float d[2],uint32_t *hit)
{
    float ax,ay,ox,oy,stored;double bx,by,denominator,t;uint32_t i;
    if(!a || !b || !c || !d || !hit)return RF_RANGE;
    for(i=0;i<2;i++)if(!isfinite(a[i]) || !isfinite(b[i]) || !isfinite(c[i]) || !isfinite(d[i]))return RF_RANGE;
    ax=(float)((double)b[0]-a[0]);ay=(float)((double)b[1]-a[1]);
    if(!isfinite(ax) || !isfinite(ay))return RF_RANGE;
    bx=(double)d[0]-c[0];by=(double)d[1]-c[1];denominator=by*ax-bx*ay;
    if(denominator==0){*hit=0;return RF_OK;}stored=(float)denominator;
    ox=(float)((double)a[0]-c[0]);oy=(float)((double)a[1]-c[1]);
    if(!isfinite(ox) || !isfinite(oy))return RF_RANGE;
    t=((double)oy*bx-(double)ox*by)/denominator;
    if(t<0 || t>1 || stored==0){*hit=0;return RF_OK;}
    t=((double)oy*ax-(double)ox*ay)/stored;
    *hit=t>=0 && t<=1;return RF_OK;
}

int rf_lightmap_texel_coverage(const rf_lightmap_uv_polygon *polygons,uint32_t count,
    const float minimum[2],const float maximum[2],uint32_t *row_seen,uint32_t *crossings)
{
    float corners[4][2];uint32_t i,j,k,total=0,hit;int status;
    static const unsigned char sides[4][2]={{0,1},{2,3},{0,2},{1,3}};
    if((count && !polygons) || !minimum || !maximum || !row_seen || !crossings)return RF_RANGE;
    for(i=0;i<2;i++)if(!isfinite(minimum[i]) || !isfinite(maximum[i]) || minimum[i]>maximum[i])return RF_RANGE;
    for(i=0;i<4;i++){corners[i][0]=(i&1)?maximum[0]:minimum[0];corners[i][1]=(i&2)?maximum[1]:minimum[1];}
    for(i=0;i<count;i++) {
        const rf_lightmap_uv_polygon *polygon=polygons+i;
        if(polygon->count && !polygon->uv)return RF_RANGE;
        for(j=0;j<polygon->count;j++)for(k=0;k<4;k++) {
            status=rf_lightmap_edge_crossing(corners[sides[k][0]],corners[sides[k][1]],
                polygon->uv[j],polygon->uv[j+1==polygon->count?0:j+1],&hit);
            if(status)return status;
            if(hit){if(total==INT32_MAX)return RF_RANGE;++total;}
        }
    }
    if(total)*row_seen=1;
    *crossings=total;return RF_OK;
}

static float lightmap_lerp_stored(float a,float b,float factor)
{
    float delta=(float)((double)b-a),scaled=(float)((double)delta*factor);
    return (float)((double)a+scaled);
}
int rf_lightmap_interpolate_edges(const rf_lightmap_sample_vertex vertices[4],const float center[2],
    rf_lightmap_special_sample *sample)
{
    float t[2],across;double x[2],span;rf_lightmap_special_sample edge[2],result;uint32_t i,j;
    if(!vertices || !center || !sample || !isfinite(center[0]) || !isfinite(center[1]))return RF_RANGE;
    for(i=0;i<4;i++) {
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_RANGE;
        for(j=0;j<3;j++)if(!isfinite(vertices[i].position[j]) || !isfinite(vertices[i].normal[j]))return RF_RANGE;
    }
    for(i=0;i<2;i++) {
        const rf_lightmap_sample_vertex *a=vertices+2*i,*b=a+1;
        span=(double)b->uv[1]-a->uv[1];if(span==0)return RF_RANGE;
        t[i]=(float)(((double)center[1]-a->uv[1])/span);
        if(!isfinite(t[i]))return RF_RANGE;
        x[i]=((double)b->uv[0]-a->uv[0])*t[i]+a->uv[0];
        for(j=0;j<3;j++) {
            edge[i].position[j]=lightmap_lerp_stored(a->position[j],b->position[j],t[i]);
            edge[i].normal[j]=lightmap_lerp_stored(a->normal[j],b->normal[j],t[i]);
        }
    }
    span=x[1]-x[0];if(span==0)span=1;
    across=(float)(((double)center[0]-x[0])/span);
    if(!isfinite(across))return RF_RANGE;
    for(j=0;j<3;j++) {
        result.position[j]=lightmap_lerp_stored(edge[0].position[j],edge[1].position[j],across);
        result.normal[j]=lightmap_lerp_stored(edge[0].normal[j],edge[1].normal[j],across);
        if(!isfinite(result.position[j]) || !isfinite(result.normal[j]))return RF_RANGE;
    }
    *sample=result;return RF_OK;
}

int rf_lightmap_select_sample(const rf_lightmap_sample_polygon *polygons,uint32_t count,
    const float center[2],float radius,rf_lightmap_special_sample *sample,uint32_t *kind)
{
    rf_lightmap_sample_vertex selected[4];float epsilon=0.0001f;uint32_t pass,i,j,k,found;int status;
    if((count && !polygons) || !center || !sample || !kind || !isfinite(center[0]) ||
       !isfinite(center[1]) || !isfinite(radius) || radius<0)return RF_RANGE;
    for(i=0;i<count;i++) {
        if(polygons[i].count && !polygons[i].vertices)return RF_RANGE;
        for(j=0;j<polygons[i].count;j++) {
            const rf_lightmap_sample_vertex *v=polygons[i].vertices+j;
            for(k=0;k<2;k++)if(!isfinite(v->uv[k]))return RF_RANGE;
            for(k=0;k<3;k++)if(!isfinite(v->position[k]) || !isfinite(v->normal[k]))return RF_RANGE;
        }
    }
    for(pass=0;pass<=10;pass++) {
        found=0;
        for(i=0;i<count && found<2;i++)for(j=0;j<polygons[i].count && found<2;j++) {
            const rf_lightmap_sample_vertex *a=polygons[i].vertices+j;
            const rf_lightmap_sample_vertex *b=polygons[i].vertices+(j+1==polygons[i].count?0:j+1);
            double ay=a->uv[1],by=b->uv[1],y=center[1];
            if(fabs(by-ay)>0.001 && ((ay-epsilon<=y && y<=by+epsilon) || (y<=ay+epsilon && by-epsilon<=y))) {
                selected[2*found]=*a;selected[2*found+1]=*b;++found;
            }
        }
        epsilon=(float)((double)epsilon*2);
        if(found==2) {
            if(pass==10)break;
            status=rf_lightmap_interpolate_edges(selected,center,sample);if(status)return status;
            *kind=2;return RF_OK;
        }
        if(pass==0)for(i=0;i<count;i++)for(j=0;j<polygons[i].count;j++) {
            const rf_lightmap_sample_vertex *v=polygons[i].vertices+j;
            double dx=(double)v->uv[0]-center[0],dy=(double)v->uv[1]-center[1];
            if(sqrt(dx*dx+dy*dy)<=radius) {
                memcpy(sample->position,v->position,sizeof(sample->position));
                memcpy(sample->normal,v->normal,sizeof(sample->normal));*kind=1;return RF_OK;
            }
        }
    }
    *kind=0;return RF_OK;
}

int rf_lightmap_copy_special_border(float *channels[3],uint32_t width,uint32_t height,uint32_t capacity)
{
    uint32_t x,y,c,at;
    if(!channels || !channels[0] || !channels[1] || !channels[2] || width<2 || height<2 ||
       (uint64_t)width*height>capacity)return RF_RANGE;
    for(y=0;y<height;y++) {
        at=y*width;
        for(c=0;c<3;c++) {
            memcpy(channels[c]+at,channels[c]+at+1,4);
            memcpy(channels[c]+at+width-1,channels[c]+at+width-2,4);
        }
    }
    for(x=0;x<width;x++)for(c=0;c<3;c++) {
        memcpy(channels[c]+x,channels[c]+width+x,4);
        memcpy(channels[c]+(height-1)*width+x,channels[c]+(height-2)*width+x,4);
    }
    return RF_OK;
}

int rf_lightmap_corner_normal(const rf_lightmap_normal_face *base,const rf_lightmap_normal_face *adjacent,
    uint32_t count,float out[3])
{
    float value[3],factor;uint32_t i,j,accepted=1;double length,inverse;
    if(!base || !out || (count && !adjacent) || count==UINT32_MAX)return RF_RANGE;
    for(j=0;j<3;j++){if(!isfinite(base->normal[j]))return RF_RANGE;value[j]=base->normal[j];}
    for(i=0;i<count;i++) {
        const rf_lightmap_normal_face *face=adjacent+i;double dot;
        if(face->id==base->id || !face->vertex_count)continue;
        for(j=0;j<3;j++)if(!isfinite(face->normal[j]))return RF_RANGE;
        dot=((double)face->normal[2]*base->normal[2]+(double)face->normal[1]*base->normal[1])+(double)face->normal[0]*base->normal[0];
        if(!(dot>0))continue;
        for(j=0;j<3;j++){value[j]=(float)((double)value[j]+face->normal[j]);if(!isfinite(value[j]))return RF_RANGE;}
        ++accepted;
    }
    factor=(float)(1.0/accepted);
    for(j=0;j<3;j++)value[j]=(float)((double)value[j]*factor);
    length=sqrt(((double)value[0]*value[0]+(double)value[1]*value[1])+(double)value[2]*value[2]);
    if(!(length>0) || !isfinite(length))return RF_RANGE;inverse=1.0/length;
    for(j=0;j<3;j++){value[j]=(float)(inverse*value[j]);if(!isfinite(value[j]))return RF_RANGE;}
    memcpy(out,value,sizeof(value));return RF_OK;
}

int rf_lightmap_mapping_read(const void *record,uint32_t bytes,uint32_t image_count,rf_lightmap_mapping *out)
{
    const unsigned char *p=record;rf_lightmap_mapping value;
    if(!record || bytes!=96 || !image_count || image_count>INT32_MAX || !out)return RF_RANGE;
    value.image=u32(p);if(value.image>=image_count || value.image>INT32_MAX)value.image=0;
    value.x=p[4];value.y=p[5];value.width=p[6];value.height=p[7];
    memcpy(value.density,p+8,8);memcpy(value.minimum,p+16,24);memcpy(value.plane,p+40,16);
    value.special=u32(p+56)!=0;value.inhibit=u32(p+60)!=0;
    value.normal_axis=u32(p+64);value.u_axis=u32(p+68);value.v_axis=u32(p+72);
    memcpy(value.scale,p+84,8);memcpy(value.offset,p+76,8);memcpy(&value.room,p+92,4);
    *out=value;return RF_OK;
}

/* 4f5219..4f5320, after the shadow ray/plane intersection. */
int rf_lightmap_project_shadow(const rf_lightmap_sample_plane *view,uint32_t width,uint32_t height,
    const float point[3],float uv[2])
{
    float value[2],scaled;uint32_t axis[2],i,extent[2];
    if(!view || !point || !uv || view->normal_axis>2 || view->u_axis>2 || view->normal_axis==view->u_axis ||
        !view->image_width || !view->image_height || view->image_width>INT32_MAX || view->image_height>INT32_MAX ||
        view->x>INT32_MAX || view->y>INT32_MAX || width<2 || height<2 || width>INT32_MAX || height>INT32_MAX)return RF_RANGE;
    axis[0]=view->u_axis;axis[1]=3-view->normal_axis-view->u_axis;extent[0]=width;extent[1]=height;
    for(i=0;i<2;i++) {
        if(!isfinite(point[axis[i]]) || !isfinite(view->scale[i]) || !isfinite(view->offset[i]))return RF_RANGE;
        value[i]=(float)((double)point[axis[i]]*view->scale[i]);
        value[i]=(float)((double)value[i]+view->offset[i]);
    }
    /* Original stores image-scaled U before subtracting origin, but retains V. */
    scaled=(float)((double)view->image_width*value[0]);value[0]=(float)((double)scaled-view->x);
    value[1]=(float)((double)view->image_height*value[1]-view->y);
    for(i=0;i<2;i++) {
        if(!isfinite(value[i]))return RF_RANGE;
        if(value[i]<1)value[i]=1;
        else if((double)value[i]>extent[i]-1)value[i]=(float)(extent[i]-1);
    }
    memcpy(uv,value,sizeof(value));return RF_OK;
}

/* 4f2100 keeps inclusive X/last-row bounds and wraps subtraction in bytes.
 * The caller must supply the explicitly checked guard row and final byte. */
int rf_lightmap_raster_shadow(const float (*vertices)[2],uint32_t count,unsigned char *mask,
    uint32_t bytes,uint32_t width,uint32_t height,unsigned char amount)
{
    uint32_t i,index[2],remaining;int32_t scan,end[2],next,row,limit,left,right;float x[2]={0,0},slope[2]={0,0};
    if(!vertices || !count || count>INT32_MAX || !mask || !width || width>=INT32_MAX || height>=INT32_MAX ||
        (uint64_t)width*(height+1)+1>bytes)return RF_RANGE;
    index[0]=0;
    for(i=0;i<count;i++) {
        if(!isfinite(vertices[i][0]) || !isfinite(vertices[i][1]) ||
            fabs((double)vertices[i][0])>INT32_MAX-2.0 || fabs((double)vertices[i][1])>INT32_MAX-2.0)return RF_RANGE;
        if(vertices[i][1]<vertices[index[0]][1])index[0]=i;
    }
    index[1]=index[0];scan=(int32_t)ceil((double)vertices[index[0]][1]-.5);
    end[0]=end[1]=scan-1;remaining=count;
    while(remaining) {
        for(i=0;i<2;i++)while(end[i]<=scan && remaining) {
            uint32_t from=index[i],to=i?(from+1==count?0:from+1):(from?from-1:count-1);
            double dy=(double)vertices[to][1]-vertices[from][1];
            end[i]=(int32_t)floor((double)vertices[to][1]+.5);if(dy==0)dy=1;
            slope[i]=(float)((1.0/dy)*((double)vertices[to][0]-vertices[from][0]));
            x[i]=(float)((((double)scan+.5)-vertices[from][1])*slope[i]+vertices[from][0]);
            if(!isfinite(slope[i]) || !isfinite(x[i]))return RF_RANGE;
            index[i]=to;remaining--;
        }
        next=end[0]<end[1]?end[0]:end[1];row=scan<0?0:scan;
        limit=next<(int32_t)(height+1)?next:(int32_t)(height+1);
        for(;row<limit;row++) {
            double lo=ceil((double)(x[0]<x[1]?x[0]:x[1])-.5);
            double hi=floor((double)(x[0]<x[1]?x[1]:x[0])-.5);
            if(!isfinite(lo) || !isfinite(hi) || lo<INT32_MIN || lo>INT32_MAX || hi<INT32_MIN || hi>INT32_MAX)return RF_RANGE;
            left=lo<0?0:(int32_t)lo;right=hi>width?(int32_t)width:(int32_t)hi;
            if(left<=right) {
                uint32_t at=(uint32_t)row*width+(uint32_t)left,n=(uint32_t)(right-left)+1;
                while(n--) {mask[at]=(unsigned char)(mask[at]-amount);at++;}
            }
            x[0]=(float)((double)x[0]+slope[0]);x[1]=(float)((double)x[1]+slope[1]);
        }
        scan=next;
    }
    return RF_OK;
}

/* 4f24a0: shadow-mask UV unprojection uses direct division, unlike the
 * ordinary texel sampler's stored reciprocal. Keep both rounding paths. */
int rf_lightmap_unproject(const rf_lightmap_sample_plane *view,const float uv[2],float point[3])
{
    float value[3],coordinates[2];uint32_t i,a,b,v;
    if(!view || !uv || !point || view->normal_axis>2 || view->u_axis>2 || view->normal_axis==view->u_axis)return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(view->plane[i]))return RF_RANGE;
    if(view->plane[view->normal_axis]==0)return RF_RANGE;
    for(i=0;i<2;i++) {
        if(!isfinite(uv[i]) || !isfinite(view->scale[i]) || !view->scale[i] || !isfinite(view->offset[i]))return RF_RANGE;
        coordinates[i]=(float)(((double)uv[i]-view->offset[i])/view->scale[i]);
        if(!isfinite(coordinates[i]))return RF_RANGE;
    }
    v=3-view->normal_axis-view->u_axis;value[view->u_axis]=coordinates[0];value[v]=coordinates[1];
    a=view->normal_axis==0?1:0;b=view->normal_axis==2?1:2;
    value[view->normal_axis]=(float)(((-(double)view->plane[a]*value[a]-(double)view->plane[b]*value[b])-view->plane[3])/view->plane[view->normal_axis]);
    if(!isfinite(value[view->normal_axis]))return RF_RANGE;
    memcpy(point,value,sizeof(value));return RF_OK;
}

int rf_lightmap_sample_position(const rf_lightmap_sample_plane *view,uint32_t x,uint32_t y,float point[3])
{
    float uv[2],value[3],step[2],inverse[2];uint32_t coordinates[2],i,a,b,v;
    if(!view || !point || !view->image_width || !view->image_height || view->image_width>INT32_MAX || view->image_height>INT32_MAX ||
        view->normal_axis>2 || view->u_axis>2 || view->normal_axis==view->u_axis ||
        (uint64_t)view->x+x>=view->image_width || (uint64_t)view->y+y>=view->image_height)return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(view->plane[i]))return RF_RANGE;
    if(view->plane[view->normal_axis]==0)return RF_RANGE;
    coordinates[0]=view->x+x;coordinates[1]=view->y+y;
    step[0]=(float)(1.0/view->image_width);step[1]=(float)(1.0/view->image_height);
    for(i=0;i<2;i++) {
        if(!isfinite(view->scale[i]) || !view->scale[i] || !isfinite(view->offset[i]))return RF_RANGE;
        inverse[i]=(float)(1.0/view->scale[i]);if(!isfinite(inverse[i]))return RF_RANGE;
        uv[i]=(float)((((double)coordinates[i]+.5)*step[i]-view->offset[i])*inverse[i]);
        if(!isfinite(uv[i]))return RF_RANGE;
    }
    v=3-view->normal_axis-view->u_axis;value[view->u_axis]=uv[0];value[v]=uv[1];
    a=view->normal_axis==0?1:0;b=view->normal_axis==2?1:2;
    value[view->normal_axis]=(float)(((-(double)view->plane[a]*value[a]-(double)view->plane[b]*value[b])-view->plane[3])/view->plane[view->normal_axis]);
    if(!isfinite(value[view->normal_axis]))return RF_RANGE;
    memcpy(point,value,sizeof(value));return RF_OK;
}

int rf_lightmap_accumulate_samples(const rf_lightmap_sample_lighting *view)
{
    unsigned char weights[64];uint32_t x,y,i,at=0;int status;
    if(!view || !view->width || !view->height || view->light_count>64 ||
        (view->light_count && !view->lights) || !view->channels[0] || !view->channels[1] || !view->channels[2] ||
        (uint64_t)view->width*view->height>view->capacity ||
        (view->masks && (uint64_t)view->width*view->height>view->mask_bytes))return RF_RANGE;
    if(view->masks)for(i=0;i<view->light_count;i++)if(!view->masks[i])return RF_RANGE;
    for(y=0;y<view->height;y++)for(x=0;x<view->width;x++,at++) {
        float point[3],rgb[3];
        status=rf_lightmap_sample_position(&view->sample,x,y,point);if(status)return status;
        for(i=0;i<3;i++)rgb[i]=view->channels[i][at];
        if(view->masks)for(i=0;i<view->light_count;i++)weights[i]=view->masks[i][at];
        status=rf_vfx_light_accumulate(point,view->sample.plane,rgb,view->directional_scale,view->lights,
            view->light_count,view->masks?weights:NULL,1,rgb);if(status)return status;
        for(i=0;i<3;i++)view->channels[i][at]=rgb[i]<0?0:rgb[i];
    }
    return RF_OK;
}

static int lightmap_ambient_values(const float global[3],const unsigned char room[4],float values[3])
{
    uint32_t c;
    for(c=0;c<3;c++) {
        if(room && room[0]==1)values[c]=(float)((double)room[c+1]*0.0039215688593685626983642578125);
        else {if(!global || !isfinite(global[c]))return RF_RANGE;values[c]=global[c];}
    }
    return RF_OK;
}
int rf_lightmap_fill_ambient(unsigned char *rgb,uint32_t bytes,uint32_t pitch,uint32_t width,uint32_t height,
    const float global[3],const unsigned char room[4],unsigned char *dirty)
{
    float values[3];unsigned char color[3];uint32_t c,x,y;int status;
    if(!rgb || !dirty || !width || !height || (uint64_t)width*3>pitch ||
       (uint64_t)(height-1)*pitch+(uint64_t)width*3>bytes)return RF_RANGE;
    status=lightmap_ambient_values(global,room,values);if(status)return status;
    for(c=0;c<3;c++) {
        double scaled=(double)values[c]*128;
        if(!isfinite(scaled) || scaled<INT32_MIN || scaled>INT32_MAX)return RF_RANGE;
        color[c]=(unsigned char)(int32_t)scaled;
    }
    for(y=0;y<height;y++)for(x=0;x<width;x++)memcpy(rgb+(uint64_t)y*pitch+x*3,color,3);
    *dirty|=8;return RF_OK;
}

int rf_lightmap_seed_ambient(const rf_lightmap_sample_lighting *view,const float global[3],const unsigned char room[4])
{
    float values[3];uint32_t i,c,count;
    if(!view || !view->width || !view->height || !view->channels[0] || !view->channels[1] ||
       !view->channels[2] || (uint64_t)view->width*view->height>view->capacity)return RF_RANGE;
    {int status=lightmap_ambient_values(global,room,values);if(status)return status;}
    for(c=0;c<3;c++)values[c]=(float)((double)values[c]*.5);
    count=view->width*view->height;
    for(c=0;c<3;c++)for(i=0;i<count;i++)view->channels[c][i]=values[c];
    return RF_OK;
}

int rf_lightmap_accumulate_special(const rf_lightmap_sample_lighting *view,
    const rf_lightmap_sample_polygon *polygons,uint32_t polygon_count)
{
    unsigned char weights[64];float inverse[2],radius;uint32_t x,y,i,j,k,c,at=0;int status;
    static const unsigned char sides[4][2]={{0,1},{2,3},{0,2},{1,3}};
    if(!view || (polygon_count && !polygons) || view->width<2 || view->height<2 ||
       view->light_count>64 || (view->light_count && !view->lights) ||
       !view->channels[0] || !view->channels[1] || !view->channels[2] ||
       (uint64_t)view->width*view->height>view->capacity ||
       (view->masks && (uint64_t)view->width*view->height>view->mask_bytes) ||
       !view->sample.image_width || !view->sample.image_height ||
       view->sample.image_width>INT32_MAX || view->sample.image_height>INT32_MAX ||
       (uint64_t)view->sample.x+view->width>view->sample.image_width ||
       (uint64_t)view->sample.y+view->height>view->sample.image_height)return RF_RANGE;
    for(i=0;i<polygon_count;i++)if(polygons[i].count && !polygons[i].vertices)return RF_RANGE;
    if(view->masks)for(i=0;i<view->light_count;i++)if(!view->masks[i])return RF_RANGE;
    inverse[0]=(float)(1.0/view->sample.image_width);inverse[1]=(float)(1.0/view->sample.image_height);
    radius=(float)(sqrt((1.0/view->sample.image_height)*inverse[1]+(double)inverse[0]*inverse[0])*.5);
    for(y=0;y<view->height;y++) {
        uint32_t seen=0;
        float low_y=(float)((double)(view->sample.y+y)*inverse[1]);
        float high_y=(float)((double)(view->sample.y+y+1)*inverse[1]);
        float center_y=(float)(((double)(view->sample.y+y)+.5)*inverse[1]);
        for(x=0;x<view->width;x++,at++) {
            float corners[4][2],center[2],rgb[3];uint32_t hit,kind=0;
            float low_x=(float)((double)(view->sample.x+x)*inverse[0]);
            float high_x=(float)((double)(view->sample.x+x+1)*inverse[0]);
            rf_lightmap_special_sample sample;
            center[0]=(float)(((double)(view->sample.x+x)+.5)*inverse[0]);center[1]=center_y;
            for(i=0;i<4;i++){corners[i][0]=(i&1)?high_x:low_x;corners[i][1]=(i&2)?high_y:low_y;}
            for(i=0;i<polygon_count;i++)for(j=0;j<polygons[i].count;j++)for(k=0;k<4;k++) {
                status=rf_lightmap_edge_crossing(corners[sides[k][0]],corners[sides[k][1]],
                    polygons[i].vertices[j].uv,polygons[i].vertices[j+1==polygons[i].count?0:j+1].uv,&hit);
                if(status)return status;if(hit)seen=1;
            }
            if(seen) {
                status=rf_lightmap_select_sample(polygons,polygon_count,center,radius,&sample,&kind);if(status)return status;
            }
            if(!seen || !kind)for(c=0;c<3;c++)rgb[c]=seen?0.33f:0.1f;
            else {
                for(c=0;c<3;c++)rgb[c]=view->channels[c][at];
                if(view->masks)for(i=0;i<view->light_count;i++)weights[i]=view->masks[i][at];
                status=rf_vfx_light_accumulate(sample.position,sample.normal,rgb,view->directional_scale,
                    view->lights,view->light_count,view->masks?weights:NULL,0,rgb);if(status)return status;
                for(c=0;c<3;c++)if(rgb[c]<0)rgb[c]=0;
            }
            for(c=0;c<3;c++)view->channels[c][at]=rgb[c];
        }
    }
    {float *channels[3]={view->channels[0],view->channels[1],view->channels[2]};
     return rf_lightmap_copy_special_border(channels,view->width,view->height,view->capacity);}
}

int rf_lightmap_live_pixel(const unsigned char base[3],const float position[3],const float normal[3],
    float directional_scale,const rf_vfx_light_source *sources,uint32_t count,uint16_t *packed)
{
    const float zero[3]={0,0,0};float accumulated[3];unsigned char rgb[3];uint32_t c,value[3];int status;
    if(!base || !packed)return RF_RANGE;
    status=rf_vfx_light_accumulate(position,normal,zero,directional_scale,sources,count,NULL,0,accumulated);if(status)return status;
    status=rf_vfx_light_rgb(accumulated,zero,-1,rgb);if(status)return status;
    for(c=0;c<3;c++){value[c]=((uint32_t)base[c]+rgb[c])>>3;if(value[c]>31)value[c]=31;}
    *packed=(uint16_t)(0x8000u|(value[0]<<10)|(value[1]<<5)|value[2]);return RF_OK;
}

int rf_lightmap_live_rectangle(const rf_lightmap_sample_lighting *lighting,const rf_lightmap_rgb_upload *view,unsigned char *dirty)
{
    uint32_t x,y;uint64_t right,bottom;int status;
    if(!lighting || !view || !dirty || !view->rgb || !view->packed || !view->width || !view->height ||
       lighting->width!=view->width || lighting->height!=view->height || lighting->sample.x!=view->x ||
       lighting->sample.y!=view->y || (view->packed_pitch&1))return RF_RANGE;
    right=(uint64_t)view->x+view->width;bottom=(uint64_t)view->y+view->height;
    if(bottom>UINT32_MAX || right*3>view->rgb_pitch || right*2>view->packed_pitch ||
       (bottom-1)*view->rgb_pitch+right*3>view->rgb_bytes ||
       (bottom-1)*view->packed_pitch+right*2>view->packed_bytes)return RF_RANGE;
    for(y=0;y<view->height;y++)for(x=0;x<view->width;x++) {
        float point[3];uint16_t pixel;
        const unsigned char *base=view->rgb+(uint64_t)(view->y+y)*view->rgb_pitch+(view->x+x)*3;
        unsigned char *out=view->packed+(uint64_t)(view->y+y)*view->packed_pitch+(view->x+x)*2;
        status=rf_lightmap_sample_position(&lighting->sample,x,y,point);if(status)return status;
        status=rf_lightmap_live_pixel(base,point,lighting->sample.plane,lighting->directional_scale,
            lighting->lights,lighting->light_count,&pixel);if(status)return status;
        out[0]=(unsigned char)pixel;out[1]=(unsigned char)(pixel>>8);
    }
    *dirty&=(unsigned char)~8u;return RF_OK;
}

static int lightmap_scaled_rgb(const double scaled[3],unsigned char rgb[3])
{
    int32_t value[3],peak=0;unsigned char result[3];uint32_t i;
    if(!rgb)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(scaled[i]) || scaled[i]<INT32_MIN || scaled[i]>INT32_MAX)return RF_RANGE;
        value[i]=(int32_t)scaled[i];if(value[i]<0)value[i]=0;if(value[i]>peak)peak=value[i];
    }
    for(i=0;i<3;i++) {
        if(peak>255) {
            uint32_t product=(uint32_t)value[i]*255u;
            int64_t signed_product=product>=0x80000000u?(int64_t)product-4294967296LL:product;
            value[i]=(int32_t)(signed_product/peak);
        }
        result[i]=(unsigned char)value[i];
    }
    memcpy(rgb,result,3);return RF_OK;
}
int rf_lightmap_accumulated_rgb(const float channels[3],unsigned char rgb[3])
{
    double scaled[3];uint32_t i;if(!channels)return RF_RANGE;
    for(i=0;i<3;i++)scaled[i]=(double)channels[i]*255.0;
    return lightmap_scaled_rgb(scaled,rgb);
}
int rf_lightmap_filtered_rgb(const rf_lightmap_accumulation *view,uint32_t x,uint32_t y,unsigned char rgb[3])
{
    double scaled[3]={0,0,0};uint32_t c,i;
    if(!view || !rgb || !view->channels[0] || !view->channels[1] || !view->channels[2] ||
        view->width<2 || view->height<2 || x>=view->width || y>=view->height ||
        (uint64_t)view->width*view->height>view->count)return RF_RANGE;
    if(x>=1 && x<view->width-1 && y>=1 && y<view->height-1) {
        uint32_t at=y*view->width+x,up=at-view->width,down=at+view->width;
        uint32_t order[9]={at+1,up+1,down+1,at-1,up-1,down-1,down,up,at};
        for(c=0;c<3;c++) {
            if(c==1){order[6]=at;order[7]=down;order[8]=up;}else{order[6]=down;order[7]=up;order[8]=at;}
            scaled[c]=view->channels[c][order[0]];
            for(i=1;i<9;i++)scaled[c]+=view->channels[c][order[i]];
            scaled[c]*=28.3333339691162109375;
        }
    } else {
        uint32_t first_x=x>1?x-1:1,last_x=x+1<view->width?x+1:view->width-1;
        uint32_t first_y=y>1?y-1:1,last_y=y+1<view->height?y+1:view->height-1,xx,yy;
        double factor=255.0/((last_x-first_x+1)*(last_y-first_y+1));
        for(yy=first_y;yy<=last_y;yy++)for(xx=first_x;xx<=last_x;xx++)
            for(c=0;c<3;c++)scaled[c]+=view->channels[c][yy*view->width+xx];
        scaled[0]*=factor;scaled[1]*=(float)factor;scaled[2]*=(float)factor;
    }
    /* Original stores red/green to binary32 before ftol; blue stays in x87. */
    scaled[0]=(float)scaled[0];scaled[1]=(float)scaled[1];
    return lightmap_scaled_rgb(scaled,rgb);
}

int rf_lightmap_resolve_rgb(const rf_lightmap_accumulation *view,unsigned char *rgb,
    uint32_t bytes,uint32_t pitch,unsigned char *dirty)
{
    uint32_t x,y;int status;
    if(!view || !rgb || !dirty || !view->width || !view->height || !view->channels[0] ||
        !view->channels[1] || !view->channels[2] || (uint64_t)view->width*view->height>view->count ||
        (uint64_t)view->width*3>pitch || (uint64_t)(view->height-1)*pitch+(uint64_t)view->width*3>bytes)return RF_RANGE;
    for(y=0;y<view->height;y++)for(x=0;x<view->width;x++) {
        unsigned char *out=rgb+(uint64_t)y*pitch+x*3;
        if(view->width>=9 && view->height>=9 && x>=2 && y>=2 && x<=view->width-3 && y<=view->height-3)
            status=rf_lightmap_filtered_rgb(view,x,y,out);
        else {
            uint32_t at=y*view->width+x;float channels[3]={view->channels[0][at],view->channels[1][at],view->channels[2][at]};
            status=rf_lightmap_accumulated_rgb(channels,out);
        }
        if(status)return status;
    }
    *dirty|=8u;return RF_OK;
}

int rf_lightmap_upload_rgb_1555(const rf_lightmap_rgb_upload *view,unsigned char *dirty)
{
    uint32_t x,y,value;uint64_t right,bottom;
    if(!view || !dirty)return RF_RANGE;
    if(!view->width || !view->height)return RF_OK;
    if(*dirty&7u)return RF_RANGE;
    if(!(*dirty&8u) || !view->packed){*dirty=0;return RF_OK;}
    right=(uint64_t)view->x+view->width;bottom=(uint64_t)view->y+view->height;
    if(!view->rgb || bottom>UINT32_MAX || (view->packed_pitch&1u) || right*3>view->rgb_pitch || right*2>view->packed_pitch ||
        (bottom-1)*view->rgb_pitch+right*3>view->rgb_bytes ||
        (bottom-1)*view->packed_pitch+right*2>view->packed_bytes)return RF_RANGE;
    for(y=0;y<view->height;y++)for(x=0;x<view->width;x++) {
        const unsigned char *source=view->rgb+((uint64_t)view->y+y)*view->rgb_pitch+((uint64_t)view->x+x)*3;
        unsigned char *target=view->packed+((uint64_t)view->y+y)*view->packed_pitch+((uint64_t)view->x+x)*2;
        value=0x8000u|((uint32_t)(source[0]>>3)<<10)|((uint32_t)(source[1]>>3)<<5)|(source[2]>>3);
        target[0]=(unsigned char)value;target[1]=(unsigned char)(value>>8);
    }
    *dirty=0;return RF_OK;
}

void rf_packed_lightmaps_close(rf_packed_lightmaps *maps)
{
    uint32_t i;if(!maps)return;
    for(i=0;i<maps->count;++i)free((void*)maps->images[i].pixels);
    free(maps->images);memset(maps,0,sizeof(*maps));
}
int rf_packed_lightmaps_open(rf_packed_lightmaps *maps,const rf_level *level,uint32_t double_rgb,uint32_t budget)
{
    const rf_level_section *section;unsigned char header[8],rgb[1536];
    uint32_t count,at=4,i;uint64_t records;int status;
    if(!maps)return RF_RANGE;memset(maps,0,sizeof(*maps));
    if(!level || level->version!=180)return RF_FORMAT;
    if(double_rgb>1 || budget<sizeof(*maps))return RF_RANGE;
    section=rf_level_find(level,0x1200);if(!section)return RF_NOT_FOUND;
    if(section->size<4)return RF_FORMAT;
    status=rf_level_read(level,section,0,header,4);if(status)return status;count=u32(header);
    if((uint64_t)count*11>section->size-4)return RF_FORMAT;
    records=sizeof(*maps)+(uint64_t)count*sizeof(*maps->images);if(records>budget)return RF_RANGE;
    if(count){maps->images=calloc(count,sizeof(*maps->images));if(!maps->images)return RF_RANGE;}
    maps->count=count;maps->allocated_bytes=(uint32_t)records;
    for(i=0;i<count;++i) {
        rf_lightmap_1555_view *image=maps->images+i;unsigned char *pixels;uint32_t count_pixels,decoded=0;
        status=RF_FORMAT;if(section->size-at<8)goto fail;
        status=rf_level_read(level,section,at,header,8);if(status)goto fail;at+=8;
        image->width=u32(header);image->height=u32(header+4);status=RF_FORMAT;
        if(!image->width || !image->height || image->width>4096 || image->height>4096)goto fail;
        count_pixels=image->width*image->height;
        if((uint64_t)count_pixels*3>section->size-at)goto fail;
        image->pitch=image->width*2;image->bytes=count_pixels*2;status=RF_RANGE;
        if(image->bytes>budget-maps->allocated_bytes)goto fail;
        pixels=malloc(image->bytes);if(!pixels)goto fail;image->pixels=pixels;maps->allocated_bytes+=image->bytes;
        while(decoded<count_pixels) {
            uint32_t n=count_pixels-decoded;if(n>512)n=512;
            status=rf_level_read(level,section,at,rgb,n*3);if(status)goto fail;
            status=rf_lightmap_pack_1555(rgb,n*3,n,1,double_rgb,pixels+decoded*2,n*2,n*2);if(status)goto fail;
            at+=n*3;decoded+=n;
        }
    }
    if(at!=section->size){status=RF_FORMAT;goto fail;}return RF_OK;
 fail:rf_packed_lightmaps_close(maps);return status;
}

uint32_t rf_lightmap_requires_brightening(uint32_t renderer_mode,uint32_t multitexture,uint32_t modulate2x)
{
    if(renderer_mode==104)return 0;
    if(renderer_mode==102)return !(multitexture&255u) || !(modulate2x&255u);
    return 1;
}
