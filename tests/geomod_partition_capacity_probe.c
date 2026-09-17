/* Diagnostic for cut14 face1248, not a passing acceptance test.
 * Captured parent support1596; outgoing edge supports:
 * 1440,1720,1720,1720,1720,1596,1440.
 * Support1596 on an edge denotes a partition diagonal in this implementation.
 * Preserve both near-adjacent corners; do not weld them by distance.
 * Exit2 reproduces the unresolved partition rejection; exit0 needs subsequent
 * full stress/closure/collision acceptance before it can be called a fix. */
#include "../src/core/geomod.c"
#include <stdio.h>
static const rf_geomod_vertex vertices[7]={
    {{-41.9889717f,-8.21230984f,-8.14658356f},{0.459871739f,0.951489568f}},
    {{-42.1432152f,-8.20581055f,-7.84488058f},{0.417471379f,0.949066818f}},
    {{-42.0267563f,-8.24616337f,-8.13064957f},{0.454307377f,0.957815647f}},
    {{-41.9208412f,-8.28286266f,-8.39054298f},{0.48780793f,0.965772271f}},
    {{-41.7388382f,-8.34592628f,-8.83713913f},{0.545374691f,0.979444802f}},
    {{-41.7388382f,-8.34592628f,-8.83714104f},{0.54537493f,0.979444861f}},
    {{-41.3957748f,-8.23730373f,-9.30689907f},{0.622938573f,0.960807025f}}
};
int main(void)
{
    rf_geomod_vertex output_vertices[64],part[7];rf_geomod_face output_faces[32],source={0,7,47,UINT32_MAX};
    partition_output out={output_vertices,output_faces,64,32,0,0,RF_FORMAT};unsigned mask,i,j,n;
    double normal[3]={0};int accepted;
    for(i=0;i<7;i++)for(j=0;j<3;j++)normal[j]+=(double)vertices[i].position[(j+1)%3]*vertices[(i+1)%7].position[(j+2)%3]-(double)vertices[i].position[(j+2)%3]*vertices[(i+1)%7].position[(j+1)%3];
    accepted=partition_polygon(&out,vertices,&source);
    printf("partition=%d faces=%u status=%d\n",accepted,out.nf,out.status);
    for(mask=1;mask<128;mask++) {
        n=0;for(i=0;i<7;i++)if(mask&(1u<<i))part[n++]=vertices[i];
        if(n>=3 && partition_valid_piece(part,n,normal))printf("VALID %u count%u\n",mask,n);
    }
    return accepted?0:2;
}
