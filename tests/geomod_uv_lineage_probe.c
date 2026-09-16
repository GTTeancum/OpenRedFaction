/* Regression probe for old-crater UV lineage. Includes implementation only to
 * reach the actual static terrain_map_pending stage; link other dependencies
 * from rf_core, but do not compile geomod.c separately into this target.
 * Runs actual shared polygon splitting/storage/mapping, not a copied normal
 * formula. This is an isolated face-stage fixture, not a complete world CSG.
 * Expected current failure: mapping changes inherited UV after rounded split.
 */
#include "../src/core/geomod.c"

static int probe(float x,uint32_t *changed)
{
    rf_geomod_vertex source[4]={
        {{-.75f,11.25f,-1},{-.125f,-.09375f}},
        {{1.25f,9.25f,-1},{-.125f,.15625f}},
        {{1.25f,9.25f,1},{.125f,.15625f}},
        {{-.75f,11.25f,1},{.125f,-.09375f}}
    },front[64],back[64],inherited[128];
    rf_geomod_face face={0,4,47,UINT32_MAX};
    rf_geomod_mesh_view mesh={source,&face,4,1,1},pending;
    rf_geomod_terrain *terrain=calloc(1,sizeof(*terrain));
    float plane[4]={1,0,0,-x};uint32_t nf=0,nb=0,i;int status;
    if(!terrain)return RF_IO;
    terrain->mapping_width=terrain->mapping_height=256;
    status=rf_geomod_polygon_split(source,4,plane,front,64,back,64,&nf,&nb);
    if(status || nf!=4 || nb!=4){if(!status)status=RF_FORMAT;goto done;}
    status=rf_geomod_storage_open(&mesh,128,8,32768,&terrain->mesh);if(status)goto done;
    status=rf_geomod_storage_begin(terrain->mesh);if(status)goto done;
    status=rf_geomod_storage_append(terrain->mesh,front,nf,47,UINT32_MAX);if(status)goto done;
    status=rf_geomod_storage_append(terrain->mesh,back,nb,47,UINT32_MAX);if(status)goto done;
    status=rf_geomod_storage_pending(terrain->mesh,&pending);if(status)goto done;
    memcpy(inherited,pending.vertices,pending.vertex_count*sizeof(*inherited));
    /* Existing endpoints and interpolated cut points retain parent Y projection
     * before mapping. Division by8 is exact binary scale here. */
    for(i=0;i<pending.vertex_count;i++) {
        float expected[2]={pending.vertices[i].position[2]*.125f,pending.vertices[i].position[0]*.125f};
        if(memcmp(expected,inherited[i].uv,8)){status=RF_FORMAT;goto done;}
    }
    status=terrain_map_pending(terrain);if(status)goto done;
    for(i=0;i<pending.vertex_count;i++) {
        const rf_geomod_vertex *v=pending.vertices+i;
        if(memcmp(inherited[i].position,v->position,12)){status=RF_FORMAT;goto done;}
        if(memcmp(inherited[i].uv,v->uv,8)) {
            printf("UV_LINEAGE_CHANGED cut %.9g corner %u position %.9g %.9g %.9g inherited %.9g %.9g mapped %.9g %.9g\n",
                x,i,v->position[0],v->position[1],v->position[2],inherited[i].uv[0],inherited[i].uv[1],v->uv[0],v->uv[1]);
            ++*changed;
        }
    }
done:
    rf_geomod_storage_close(&terrain->mesh);free(terrain);return status;
}
int main(void)
{
    static const float cuts[]={0,.1f,.2f,.3f};uint32_t i,total=0;
    for(i=0;i<sizeof(cuts)/sizeof(cuts[0]);i++) {
        uint32_t changed=0;int status=probe(cuts[i],&changed);
        if(status){fprintf(stderr,"UV_LINEAGE_SETUP_ERROR %.9g %d\n",cuts[i],status);return 2;}
        if(!i && changed){fprintf(stderr,"Exact split control changed UV\n");return 2;}
        total+=changed;
    }
    if(total){fprintf(stderr,"FAIL inherited generated UV changed at %u corners\n",total);return 1;}
    puts("PASS old-generated UV lineage survives shared split/mapping");return 0;
}
