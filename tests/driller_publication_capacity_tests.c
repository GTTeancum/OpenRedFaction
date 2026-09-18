#include "rf/geomod_publication.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller publication line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_geomod_publication_work work;
static rf_geomod_template shape;
static rf_geomod_vertex vertices[RF_GEOMOD_PUBLICATION_VERTICES];
static rf_geomod_face faces[RF_GEOMOD_PUBLICATION_FACES];
static rf_geomod_publication_origin origins[RF_GEOMOD_PUBLICATION_FACES];
int main(int argc,char **argv)
{
    const char *path=argc>1?argv[1]:"build/data/driller-double.bin";
    rf_geomod_publication_cut cut={0};rf_geomod_publication_job job={0};rf_geomod_mesh_view result;
    rf_geomod_vertex neighbor[4]={0};rf_geomod_face nf={0,4,7,90};
    rf_geomod_publication_origin no={RF_GEOMOD_PUBLICATION_NEIGHBOR,55,90,100};
    const float points[4][3]={{-10,0,-10},{-10,0,10},{10,0,10},{10,0,-10}};
    const float planes[6][4]={{-1,0,0,-10},{1,0,0,-10},{0,-1,0,-10},{0,1,0,-10},{0,0,-1,-10},{0,0,1,-10}};
    const float floor_planes[6][4]={{-1,0,0,-10},{1,0,0,-10},{0,-1,0,-10},{0,1,0,0},{0,0,-1,-10},{0,0,1,-10}};
    rf_geomod_publication_solid floor={floor_planes,6,55};
    double area=0;uint32_t i,j,craters=0;
    CHECK(!rf_geomod_template_load(path,&shape) && shape.face_count==64);
    cut.mesh=(rf_geomod_mesh_view){shape.vertices,shape.faces,192,64,0};cut.star=1;memcpy(cut.kernel,shape.kernel,12);
    for(i=0;i<4;i++)memcpy(neighbor[i].position,points[i],12);
    job.terrain=cut.mesh;job.neighbors=(rf_geomod_mesh_view){neighbor,&nf,4,1,0};job.neighbor_origins=&no;
    job.source_planes=planes;job.source_plane_count=6;job.cuts=&cut;job.cut_count=1;job.solids=&floor;job.solid_count=1;
    job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,99,UINT32_MAX,101};
    CHECK(!rf_geomod_publication_build(&job,&work,vertices,RF_GEOMOD_PUBLICATION_VERTICES,faces,RF_GEOMOD_PUBLICATION_FACES,origins,&result));
    CHECK(work.tetra_count==64);
    for(i=0;i<result.face_count;i++){
        const rf_geomod_face *f=result.faces+i;
        if(origins[i].kind==RF_GEOMOD_PUBLICATION_CRATER){++craters;CHECK(origins[i].owner==99);}
        if(origins[i].kind==RF_GEOMOD_PUBLICATION_NEIGHBOR){double polygon=0;CHECK(origins[i].owner==55);
            for(j=0;j<f->count;j++){const float *a=result.vertices[f->first+j].position,*b=result.vertices[f->first+(j+1)%f->count].position;polygon+=(double)a[0]*b[2]-(double)b[0]*a[2];}
            area+=fabs(polygon)*.5;}
    }
    CHECK(craters>0 && area>0 && area<399.9);
    printf("PASS actual64-face Driller publication,64tetra,exposed neighbor area%.9g faces%u work%u\n",area,result.face_count,(unsigned)sizeof(work));return 0;
}
