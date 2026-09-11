#include "pc_raster.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static float edge(const float *a, const float *b, float x, float y) { return (x-a[0])*(b[1]-a[1])-(y-a[1])*(b[0]-a[0]); }
static int address(int value, int size, int clamp)
{ return clamp ? (value < 0 ? 0 : value >= size ? size-1 : value) : (value % size + size) % size; }
static void sample(const rf_image *image, float s, float t, int clamp, float color[4])
{
    float x, y, fx, fy;
    int ix, iy, a, b;
    unsigned c;
    s = clamp ? fminf(1, fmaxf(0, s)) : s-floorf(s);
    t = clamp ? fminf(1, fmaxf(0, t)) : t-floorf(t);
    x = s*image->width-0.5f; y = t*image->height-0.5f;
    ix = (int)floorf(x); iy = (int)floorf(y); fx = x-ix; fy = y-iy;
    for (c = 0; c < 4; ++c) color[c] = 0;
    for (b = 0; b < 2; ++b) for (a = 0; a < 2; ++a) {
        float weight = (a ? fx : 1-fx)*(b ? fy : 1-fy);
        unsigned pixel = (unsigned)(address(iy+b, (int)image->height, clamp)*(int)image->width + address(ix+a, (int)image->width, clamp));
        for (c = 0; c < 4; ++c) color[c] += weight*image->rgba[pixel*4+c]/255.0f;
    }
}

int rf_pc_raster_open(rf_pc_raster *r,uint32_t scale)
{
    rf_pc_raster value={0};
    if(!r || r->rgb || r->depth || scale<1 || scale>4)return RF_RANGE;
    value.scale=scale;value.width=640*scale;value.height=480*scale;value.pixels=value.width*value.height;
    value.depth=malloc(value.pixels*sizeof(float));value.rgb=calloc(value.pixels,3);
    if(!value.depth || !value.rgb){free(value.depth);free(value.rgb);return RF_IO;}
    *r=value;return RF_OK;
}
void rf_pc_raster_close(rf_pc_raster *r)
{if(r){free(r->depth);free(r->rgb);memset(r,0,sizeof(*r));}}
int rf_pc_raster_frame(rf_pc_raster *r,const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,uint32_t world_vertices)
{
    uint32_t i,width,height,pixels,scale;float *depth;unsigned char *rgb;
    if(!r || !r->rgb || !r->depth || !mesh || !materials || !lightmaps ||
       world_vertices>mesh->count || mesh->count%3 || world_vertices%3 ||
       mesh->bytes!=(uint64_t)mesh->count*sizeof(rf_preview_vertex))return RF_RANGE;
    width=r->width;height=r->height;pixels=r->pixels;scale=r->scale;depth=r->depth;rgb=r->rgb;
    for (i = 0; i < pixels; ++i) { depth[i] = 16777216; rgb[i*3] = 16; rgb[i*3+1] = 16; rgb[i*3+2] = 24; }
    for (i = 0; i + 2 < mesh->count; i += 3) {
        int actor_triangle=i>=world_vertices;
        const rf_preview_vertex *a = mesh->vertices+i, *b = a+1, *c = a+2;
        rf_preview_vertex scaled[3];
        if(scale!=1){unsigned j;memcpy(scaled,a,sizeof(scaled));for(j=0;j<3;++j){scaled[j].position[0]*=scale;scaled[j].position[1]*=scale;}a=scaled;b=scaled+1;c=scaled+2;}
        float area = edge(a->position,b->position,c->position[0],c->position[1]);
        int x, y, xmin, xmax, ymin, ymax;
        if (fabsf(area) < 0.00001f) continue;
        xmin = (int)floorf(fminf(a->position[0],fminf(b->position[0],c->position[0])));
        xmax = (int)ceilf(fmaxf(a->position[0],fmaxf(b->position[0],c->position[0])));
        ymin = (int)floorf(fminf(a->position[1],fminf(b->position[1],c->position[1])));
        ymax = (int)ceilf(fmaxf(a->position[1],fmaxf(b->position[1],c->position[1])));
        if (xmin < 0) xmin = 0; if (xmax >= (int)width) xmax = (int)width-1;
        if (ymin < 0) ymin = 0; if (ymax >= (int)height) ymax = (int)height-1;
        for (y = ymin; y <= ymax; ++y) for (x = xmin; x <= xmax; ++x) {
            float u = edge(b->position,c->position,x+0.5f,y+0.5f)/area;
            float v = edge(c->position,a->position,x+0.5f,y+0.5f)/area;
            float w = 1-u-v, z;
            uint32_t pixel = (uint32_t)(y*width+x), channel;
            if (u < 0 || v < 0 || w < 0) continue;
            z = u*a->position[2]+v*b->position[2]+w*c->position[2];
            if (z >= depth[pixel]) continue;
            if(!actor_triangle)depth[pixel] = z;
            if(!actor_triangle || !materials->count)for (channel = 0; channel < 3; ++channel) rgb[pixel*3+channel] = (unsigned char)(a->color[channel]*255);
            if (materials->count) {
                const rf_image *image = a->material < materials->count && materials->items[a->material].status == RF_OK ? &materials->items[a->material].image : NULL;
                float q = u*a->texture[2]+v*b->texture[2]+w*c->texture[2];
                float s = (u*a->texture[0]+v*b->texture[0]+w*c->texture[0])/q;
                float t = (u*a->texture[1]+v*b->texture[1]+w*c->texture[1])/q;
                float base[4]={1,1,1,1}, light[4] = {0.5f, 0.5f, 0.5f,1};
                if (image) sample(image, s, t, 0, base);
                else for (channel = 0; channel < 3; ++channel) base[channel] = a->color[channel];
                if (a->lightmap < lightmaps->count) {
                    float ls = (u*a->lightmap_texture[0]+v*b->lightmap_texture[0]+w*c->lightmap_texture[0])/q;
                    float lt = (u*a->lightmap_texture[1]+v*b->lightmap_texture[1]+w*c->lightmap_texture[1])/q;
                    sample(lightmaps->images+a->lightmap, ls, lt, 1, light);
                }
                if(actor_triangle && base[3]>=1)depth[pixel]=z;
                for (channel = 0; channel < 3; ++channel) {
                    float color=fminf(1,base[channel]*light[channel]*2)*255;
                    if(actor_triangle)color=color*base[3]+rgb[pixel*3+channel]*(1-base[3]);
                    rgb[pixel*3+channel]=(unsigned char)floorf(color+0.5f);
                }
            }
        }
    }
    return RF_OK;
}
int rf_pc_raster_save(const rf_pc_raster *r,const char *path)
{
    FILE *output;int failed;
    if(!r || !r->rgb || !path)return RF_RANGE;
    output=fopen(path,"wb");if(!output)return RF_IO;
    failed=fprintf(output,"P6\n%u %u\n255\n",r->width,r->height)<0;
    if(fwrite(r->rgb,3,r->pixels,output)!=r->pixels)failed=1;
    if(fclose(output))failed=1;return failed?RF_IO:RF_OK;
}

static double particle_edge(const float *a,const float *b,double x,double y)
{return (x-a[0])*((double)b[1]-a[1])-(y-a[1])*((double)b[0]-a[0]);}
int rf_pc_raster_particle(rf_pc_raster *r,const rf_particle_draw_vertex *vertices,
    uint32_t count,const rf_image *image,uint32_t mode,float depth_scale,float depth_bias,
    uint32_t fog_enabled,uint32_t fog_rgb)
{
    float positions[12][2],colors[12][4],fog[12],xmin=INFINITY,xmax=-INFINITY,ymin=INFINITY,ymax=-INFINITY;
    uint32_t i,j,base_mode=mode&~(31u<<20),depth_mode=(mode>>20)&31u,glow,solid=mode==0x18000u;
    int x,y,x0,x1,y0,y1;
    if(!r || !r->rgb || !r->depth || !r->width || !r->height || !vertices || count<3 || count>12 ||
       (!solid && (!image || !image->rgba || !image->width || !image->height)) ||
       !isfinite(depth_scale) || !isfinite(depth_bias))return RF_RANGE;
    if((base_mode!=(RF_PARTICLE_NORMAL_MODE&~(31u<<20)) && base_mode!=(RF_PARTICLE_GLOW_MODE&~(31u<<20)) && !solid) || depth_mode>1)return RF_NOT_FOUND;
    if(!solid && ((image->width&(image->width-1)) || (image->height&(image->height-1)) ||
       (uint64_t)image->width*image->height*4>image->bytes))return RF_FORMAT;
    glow=base_mode==(RF_PARTICLE_GLOW_MODE&~(31u<<20));
    if(solid && (fog_enabled&255u))return RF_NOT_FOUND;
    for(i=0;i<count;i++) {
        if(!isfinite(vertices[i].depth) || !isfinite(vertices[i].reciprocal_w) || !isfinite(depth_bias+depth_scale*vertices[i].depth))return RF_RANGE;
        fog[i]=(!solid && !glow && (fog_enabled&255u))?(float)(vertices[i].fog>>24)/255.0f:1;
        for(j=0;j<2;j++) {
            positions[i][j]=vertices[i].screen[j]*r->scale;
            if(!isfinite(positions[i][j]) || !isfinite(vertices[i].uv[j]) || !isfinite(vertices[i].uv[j]*vertices[i].reciprocal_w))return RF_RANGE;
        }
        for(j=0;j<3;j++)colors[i][j]=(float)((vertices[i].argb>>(16-j*8))&255u)/255.0f*fog[i];
        colors[i][3]=(float)(vertices[i].argb>>24)/255.0f;
        xmin=fminf(xmin,positions[i][0]);xmax=fmaxf(xmax,positions[i][0]);
        ymin=fminf(ymin,positions[i][1]);ymax=fmaxf(ymax,positions[i][1]);
    }
    if(xmax<0 || ymax<0 || xmin>=r->width || ymin>=r->height)return RF_OK;
    x0=(int)fmaxf(0,floorf(xmin));x1=(int)fminf((float)r->width-1,ceilf(xmax));
    y0=(int)fmaxf(0,floorf(ymin));y1=(int)fminf((float)r->height-1,ceilf(ymax));
    for(y=y0;y<=y1;y++)for(x=x0;x<=x1;x++) {
        /* Cover a convex fan once, including shared internal edges. Applying
         * blending separately to both triangles would produce a dark seam. */
        for(i=1;i+1<count;i++) {
            double area=particle_edge(positions[0],positions[i],positions[i+1][0],positions[i+1][1]);
            float weights[3],q=0,z=0,uv[2]={0},color[4]={0},f=0,texel[4];uint32_t ids[3]={0,i,i+1},k,pixel;
            if(fabs(area)<1e-10)continue;
            weights[0]=(float)(particle_edge(positions[i],positions[i+1],x+0.5,y+0.5)/area);
            weights[1]=(float)(particle_edge(positions[i+1],positions[0],x+0.5,y+0.5)/area);
            weights[2]=1-weights[0]-weights[1];
            if(weights[0]<0 || weights[1]<0 || weights[2]<0)continue;
            for(j=0;j<3;j++) {
                const rf_particle_draw_vertex *v=vertices+ids[j];float w=weights[j];
                q+=w*v->reciprocal_w;z+=w*v->depth;f+=w*fog[ids[j]];
                for(k=0;k<2;k++)uv[k]+=w*v->uv[k]*v->reciprocal_w;
                for(k=0;k<4;k++)color[k]+=w*colors[ids[j]][k];
            }
            pixel=(uint32_t)y*r->width+(uint32_t)x;
            if(depth_mode && depth_bias+depth_scale*z>r->depth[pixel])break;
            if(q==0 || !isfinite(q))break;
            if(solid)for(k=0;k<4;k++)texel[k]=1;
            else sample(image,uv[0]/q,uv[1]/q,1,texel);
            for(k=0;k<3;k++) {
                float alpha=texel[3]*color[3];
                float source=fminf(1,fmaxf(0,texel[k]*color[k]+(float)((fog_rgb>>(k*8))&255u)/255.0f*(1-f)));
                float value=source*255*alpha+r->rgb[pixel*3+k]*(glow?1:1-alpha);
                r->rgb[pixel*3+k]=(unsigned char)floorf(fminf(255,fmaxf(0,value))+0.5f);
            }
            break;
        }
    }
    return RF_OK;
}
