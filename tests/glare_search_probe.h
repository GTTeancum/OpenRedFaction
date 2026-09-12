/* Binary fixture for full glare visibility search, stable identity tokens. */
#define GLARE_TEST_BASE 0x30000000u
typedef struct glare_search_event {uint32_t operation,token;unsigned char payload[84];} glare_search_event;
typedef struct glare_search_fixture {
    uint32_t words[30],count;glare_search_event events[64];
    rf_glare_visibility_object movers[2],actors[2],selected;
} glare_search_fixture;
static int glare_search_record(glare_search_fixture *f,uint32_t operation,uint32_t token,const void *data,uint32_t bytes)
{
    glare_search_event *event;if(f->count==64)return RF_RANGE;
    event=&f->events[f->count++];memset(event,0,sizeof(*event));event->operation=operation;event->token=token;
    if(bytes)memcpy(event->payload,data,bytes);return f->words[23]==f->count?RF_IO:RF_OK;
}
static int glare_search_lookup(void *context,uint32_t handle,const rf_glare_visibility_object **out)
{
    glare_search_fixture *f=context;int status=glare_search_record(f,0,handle,NULL,0);if(status)return status;
    *out=handle==41?&f->actors[0]:handle==42?&f->actors[1]:NULL;return RF_OK;
}
static int glare_search_solid_owner(void *context,uint32_t token,const rf_glare_visibility_object **out)
{
    glare_search_fixture *f=context;int status=glare_search_record(f,1,token,NULL,0);if(status)return status;
    *out=token==GLARE_TEST_BASE+0x2000?&f->movers[0]:token==GLARE_TEST_BASE+0x3000?&f->movers[1]:NULL;return RF_OK;
}
static int glare_search_room(void *context,const rf_glare_visibility_object *object,uint32_t *out)
{
    glare_search_fixture *f=context;int status=glare_search_record(f,2,object->geometry.token,NULL,0);if(status)return status;
    *out=object==&f->selected?7:f->words[15+(object->handle==42)];return RF_OK;
}
static int glare_search_state(void *context,const rf_glare_visibility_object *object,uint32_t *out)
{
    glare_search_fixture *f=context;int status=glare_search_record(f,3,object->geometry.token,NULL,0);if(status)return status;
    *out=object==&f->selected?f->words[19]:f->words[17+(object->handle==42)];return RF_OK;
}
static int glare_search_associated(void *context,const rf_glare_visibility_object *object,const rf_glare_visibility_object **out)
{
    glare_search_fixture *f=context;int status=glare_search_record(f,4,object->geometry.token,NULL,0);if(status)return status;
    *out=f->words[20]?&f->actors[f->words[20]-1]:NULL;return RF_OK;
}
static int glare_search_solid(void *context,uint32_t token,const rf_glare_solid_query *query,rf_collision_solid_response_hit *hit,uint32_t reset)
{
    glare_search_fixture *f=context;int status;if(reset!=1)return RF_RANGE;
    status=glare_search_record(f,5,token,query,sizeof(*query));if(status)return status;
    memset(hit,0,sizeof(*hit));hit->count=(int32_t)f->words[token==99?9:7+token-101];hit->face=0x1234;return RF_OK;
}
static int glare_search_model(void *context,const rf_collision_visibility_object *object,rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted)
{
    glare_search_fixture *f=context;int status;(void)hit;if(reset!=1)return RF_RANGE;
    status=glare_search_record(f,6,object->token,&query->input,80);if(status)return status;
    *accepted=f->words[5+(object->token==GLARE_TEST_BASE+0x5000)];return RF_OK;
}
static int glare_search_probe(void)
{
    static glare_search_fixture fixture;rf_glare_base_owner glare;const float camera[3]={0,0,-4};
    const float bases[3][9]={{1,0,0,0,1,0,0,0,1},{0,0,1,0,1,0,-1,0,0},{.36f,.48f,.8f,-.8f,.6f,0,-.48f,-.64f,.6f}};uint32_t i,j,visible;int status;
    rf_glare_visibility_list movers={fixture.movers,0},actors={fixture.actors,0};
    rf_glare_visibility_backend backend={glare_search_lookup,glare_search_solid_owner,glare_search_room,glare_search_state,
        glare_search_associated,glare_search_solid,glare_search_model,&fixture};
    _Static_assert(sizeof(rf_glare_visibility_object)==96,"visibility object wire");
    _Static_assert(sizeof(rf_glare_solid_query)==84,"solid query wire");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(fixture.words,sizeof(fixture.words),1,stdin)==1) {
        uint32_t *w=fixture.words;if(w[0]>2 || w[1]>2 || w[20]>2)return 2;
        fixture.count=0;memset(&glare,0,sizeof(glare));glare.state.occluder=(int32_t)w[2];glare.state.cached_solid=w[3];
        glare.state.cached_face=w[4];glare.parent_handle=w[21];glare.position[2]=4;visible=0xa5a5a5a5;
        movers.count=w[0];actors.count=w[1];memset(&fixture.selected,0,sizeof(fixture.selected));fixture.selected.geometry.token=GLARE_TEST_BASE+0x6000;
        for(i=0;i<4;++i) {
            rf_glare_visibility_object *o=i<2?fixture.movers+i:fixture.actors+i-2;uint32_t n=i%2;
            uint32_t outside=w[(i<2?12:26)+n];memset(o,0,sizeof(*o));o->geometry.token=GLARE_TEST_BASE+0x2000+i*0x1000;
            o->handle=41+n;o->solid=101+n;o->geometry.flags=w[(i<2?10:24)+n];o->geometry.model=(void *)(uintptr_t)(i>=2?w[28+n]:0);
            for(j=0;j<3;++j){o->geometry.position[j]=(float)(j+1);o->geometry.minimum[j]=outside?20:-10;o->geometry.maximum[j]=outside?30:10;}
            memcpy(o->geometry.matrix,bases[w[19]%3],36);
        }
        status=rf_glare_visibility_search(&glare,camera,&movers,&actors,w[14]?&fixture.selected:NULL,99,w[22],&backend,&visible);
        fwrite(&status,4,1,stdout);fwrite(&visible,4,1,stdout);fwrite(&glare.state.occluder,4,1,stdout);
        fwrite(&glare.state.cached_solid,4,1,stdout);fwrite(&glare.state.cached_face,4,1,stdout);fwrite(&fixture.count,4,1,stdout);
        fwrite(fixture.events,sizeof(glare_search_event),fixture.count,stdout);
    }
    return 0;
}
