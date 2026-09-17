/* CPU-only execution of the actual scene subdivision path. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"draw capacity line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    enum { FACES=1500, VERTICES=FACES*3 };
    scene_stream s={0};rf_geomod_terrain_view source={0};uint32_t i,j;
    rf_geomod_vertex *v=calloc(VERTICES,sizeof(*v)),*saved=malloc(VERTICES*sizeof(*v));
    rf_geomod_face *faces=calloc(FACES,sizeof(*faces));rf_collision_face *bound=calloc(FACES,sizeof(*bound));
    scene_terrain_draw_mesh *before=malloc(sizeof(*before));
    s.terrain_draw=calloc(1,sizeof(*s.terrain_draw));CHECK(v && saved && faces && bound && before && s.terrain_draw);
    for(i=0;i<FACES;i++) {
        v[3*i].position[0]=(float)(i*3);v[3*i+1].position[0]=(float)(i*3+1);v[3*i+2].position[0]=(float)(i*3);
        v[3*i+2].position[1]=1;v[3*i+1].uv[0]=1;v[3*i+2].uv[1]=1;
        faces[i].first=3*i;faces[i].count=3;faces[i].material=7;faces[i].source_face=i;
        bound[i].count=3;bound[i].plane[2]=1;
    }
    /* A late source index creates a T-junction on the first face. */
    v[VERTICES-3].position[0]=.5f;v[VERTICES-3].uv[0]=99;
    v[VERTICES-2].position[2]=v[VERTICES-1].position[2]=10;
    memcpy(saved,v,VERTICES*sizeof(*v));
    source.mesh=(rf_geomod_mesh_view){v,faces,VERTICES,FACES,19};source.faces=bound;
    memcpy(before,s.terrain_draw,sizeof(*before));
    CHECK(!scene_terrain_subdivide_mode(&s,&source,0));CHECK(!memcmp(before,s.terrain_draw,sizeof(*before)));
    CHECK(!scene_terrain_subdivide(&s,&source));
    CHECK(s.terrain_draw->view.vertex_count==VERTICES+1 && s.terrain_draw->view.face_count==FACES);
    CHECK(s.terrain_draw->faces[0].count==4 && s.terrain_draw->vertices[1].position[0]==.5f && s.terrain_draw->vertices[1].uv[0]==.5f);
    CHECK(!memcmp(saved,v,VERTICES*sizeof(*v)) && faces[0].count==3);
    for(i=0;i<FACES;i++) {
        CHECK(s.terrain_draw->faces[i].source_face==i && s.terrain_draw->faces[i].material==7 && !s.terrain_draw->bound[i].vertices);
        if(i)CHECK(!memcmp(s.terrain_draw->vertices+s.terrain_draw->faces[i].first,v+3*i,3*sizeof(*v)));
    }
    for(j=0;j<3;j++)for(i=0;i<VERTICES;i++){uint16_t index=s.terrain_draw->sorted[j][i];CHECK(index<VERTICES);if(i)CHECK(v[s.terrain_draw->sorted[j][i-1]].position[j]<=v[index].position[j]);}
    memcpy(before,s.terrain_draw,sizeof(*before));source.mesh.vertex_count=SCENE_TERRAIN_SOURCE_VERTICES+1;
    CHECK(scene_terrain_subdivide(&s,&source)==RF_RANGE && !memcmp(before,s.terrain_draw,sizeof(*before)));
    source.mesh.vertex_count=VERTICES;source.mesh.face_count=SCENE_TERRAIN_FACES+1;
    CHECK(scene_terrain_subdivide(&s,&source)==RF_RANGE && !memcmp(before,s.terrain_draw,sizeof(*before)));
    printf("PASS expanded scene subdivision: %u source vertices, %u faces, exact late-index T-junction UV; bytes%u budget%u\n",VERTICES,FACES,rf_scene_terrain_draw[3],SCENE_TERRAIN_DRAW_BUDGET);
    free(s.terrain_draw);free(before);free(bound);free(faces);free(saved);free(v);return 0;
}
