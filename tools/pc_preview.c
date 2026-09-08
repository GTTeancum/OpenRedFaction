#include "rf/preview.h"
#include "rf/material.h"
#include "rf/lightmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static float edge(const float *a, const float *b, float x, float y) { return (x-a[0])*(b[1]-a[1])-(y-a[1])*(b[0]-a[0]); }
static int address(int value, int size, int clamp)
{ return clamp ? (value < 0 ? 0 : value >= size ? size-1 : value) : (value % size + size) % size; }
static void sample(const rf_image *image, float s, float t, int clamp, float color[3])
{
    float x, y, fx, fy;
    int ix, iy, a, b;
    unsigned c;
    s = clamp ? fminf(1, fmaxf(0, s)) : s-floorf(s);
    t = clamp ? fminf(1, fmaxf(0, t)) : t-floorf(t);
    x = s*image->width-0.5f; y = t*image->height-0.5f;
    ix = (int)floorf(x); iy = (int)floorf(y); fx = x-ix; fy = y-iy;
    for (c = 0; c < 3; ++c) color[c] = 0;
    for (b = 0; b < 2; ++b) for (a = 0; a < 2; ++a) {
        float weight = (a ? fx : 1-fx)*(b ? fy : 1-fy);
        unsigned pixel = (unsigned)(address(iy+b, (int)image->height, clamp)*(int)image->width + address(ix+a, (int)image->width, clamp));
        for (c = 0; c < 3; ++c) color[c] += weight*image->rgba[pixel*4+c]/255.0f;
    }
}
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_level level;
    rf_geometry geometry;
    rf_preview_mesh mesh;
    rf_materials materials = {0};
    rf_lightmaps lightmaps = {0};
    float *depth;
    unsigned char *rgb;
    uint32_t i;
    FILE *output;
    if (argc < 4 || argc > 20) return 2;
    if (rf_vpp_open(&archive, argv[1]) || rf_level_open(&level, &archive, argv[2]) ||
        rf_geometry_open(&geometry, &level, 8*1024*1024) || rf_preview_build(&mesh, &geometry, &level, 8*1024*1024)) return 1;
    if (argc > 4) {
        rf_vpp archives[16];
        uint32_t opened = 0;
        int result = RF_OK;
        for (i = 4; i < (uint32_t)argc; ++i) {
            result = rf_vpp_open(archives+opened, argv[i]);
            if (result) break;
            ++opened;
        }
        if (!result) result = rf_materials_open(&materials, &geometry, archives, opened, 4*1024*1024);
        while (opened) rf_vpp_close(archives + --opened);
        if (result) { rf_preview_close(&mesh); rf_geometry_close(&geometry); rf_vpp_close(&archive); return 1; }
        if (rf_lightmaps_open(&lightmaps, &level, 4*1024*1024)) return 1;
    }
    depth = malloc(640*480*sizeof(float)); rgb = malloc(640*480*3);
    if (!depth || !rgb) return 1;
    for (i = 0; i < 640*480; ++i) { depth[i] = 16777216; rgb[i*3] = 16; rgb[i*3+1] = 16; rgb[i*3+2] = 24; }
    for (i = 0; i + 2 < mesh.count; i += 3) {
        const rf_preview_vertex *a = mesh.vertices+i, *b = a+1, *c = a+2;
        float area = edge(a->position,b->position,c->position[0],c->position[1]);
        int x, y, xmin, xmax, ymin, ymax;
        if (fabsf(area) < 0.00001f) continue;
        xmin = (int)floorf(fminf(a->position[0],fminf(b->position[0],c->position[0])));
        xmax = (int)ceilf(fmaxf(a->position[0],fmaxf(b->position[0],c->position[0])));
        ymin = (int)floorf(fminf(a->position[1],fminf(b->position[1],c->position[1])));
        ymax = (int)ceilf(fmaxf(a->position[1],fmaxf(b->position[1],c->position[1])));
        if (xmin < 0) xmin = 0; if (xmax > 639) xmax = 639;
        if (ymin < 0) ymin = 0; if (ymax > 479) ymax = 479;
        for (y = ymin; y <= ymax; ++y) for (x = xmin; x <= xmax; ++x) {
            float u = edge(b->position,c->position,x+0.5f,y+0.5f)/area;
            float v = edge(c->position,a->position,x+0.5f,y+0.5f)/area;
            float w = 1-u-v, z;
            uint32_t pixel = (uint32_t)(y*640+x), channel;
            if (u < 0 || v < 0 || w < 0) continue;
            z = u*a->position[2]+v*b->position[2]+w*c->position[2];
            if (z >= depth[pixel]) continue;
            depth[pixel] = z;
            for (channel = 0; channel < 3; ++channel) rgb[pixel*3+channel] = (unsigned char)(a->color[channel]*255);
            if (materials.count) {
                const rf_image *image = a->material < materials.count && materials.items[a->material].status == RF_OK ? &materials.items[a->material].image : NULL;
                float q = u*a->texture[2]+v*b->texture[2]+w*c->texture[2];
                float s = (u*a->texture[0]+v*b->texture[0]+w*c->texture[0])/q;
                float t = (u*a->texture[1]+v*b->texture[1]+w*c->texture[1])/q;
                float base[3], light[3] = {0.5f, 0.5f, 0.5f};
                if (image) sample(image, s, t, 0, base);
                else for (channel = 0; channel < 3; ++channel) base[channel] = a->color[channel];
                if (a->lightmap < lightmaps.count) {
                    float ls = (u*a->lightmap_texture[0]+v*b->lightmap_texture[0]+w*c->lightmap_texture[0])/q;
                    float lt = (u*a->lightmap_texture[1]+v*b->lightmap_texture[1]+w*c->lightmap_texture[1])/q;
                    sample(lightmaps.images+a->lightmap, ls, lt, 1, light);
                }
                for (channel = 0; channel < 3; ++channel) rgb[pixel*3+channel] = (unsigned char)floorf(fminf(1, base[channel]*light[channel]*2)*255+0.5f);
            }
        }
    }
    output = fopen(argv[3],"wb");
    if (!output) return 1;
    fprintf(output,"P6\n640 480\n255\n");
    if (fwrite(rgb,3,640*480,output) != 640*480 || fclose(output)) return 1;
    printf("Prepared %u triangles (%u bytes) at original spawn\n",mesh.count/3,mesh.bytes);
    free(depth); free(rgb); rf_lightmaps_close(&lightmaps); rf_materials_close(&materials); rf_preview_close(&mesh); rf_geometry_close(&geometry); rf_vpp_close(&archive);
    return 0;
}
