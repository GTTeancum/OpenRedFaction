#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"projectile prop line%d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    float vertices[3][3]={{-2,-2,0},{0,2,0},{2,-2,0}},planes[1][4]={{0,0,-1,0}};
    rf_collision_model_triangle_record triangle={{0,1,2},0};
    rf_collision_model_batch_view batch={vertices,planes,&triangle,0,1};
    rf_collision_model_lod_view lod={&batch,0,1};
    rf_collision_model_part_view part={{0,0,0},{-2,-2,-.01f},{2,2,.01f},&lod,&lod};
    campaign_clutter_model model={0};rf_clutter_shared_static_model shared={0};
    rf_clutter_base_owner owner={0},*owners[1]={&owner};uint32_t model_slot=0,matched=0,liquid=1,handled;
    float start[3]={0,0,0},delta[3]={0,0,10};rf_weapon_flight_contact hit={0};
    rf_clutter_class definition={0};int status;
    definition.flags=2;owner.state.definition=&definition;owner.state.position[2]=5;
    owner.matrix[0]=owner.matrix[4]=owner.matrix[8]=1;
    owner.body.state.bounds.minimum[0]=owner.body.state.bounds.minimum[1]=-2;owner.body.state.bounds.minimum[2]=4.99f;
    owner.body.state.bounds.maximum[0]=owner.body.state.bounds.maximum[1]=2;owner.body.state.bounds.maximum[2]=5.01f;
    campaign_clutter_bodies=owners;campaign_clutter_records.count=1;
    campaign_clutter_models=&model;campaign_clutter_shared=&shared;campaign_clutter_model_slots=&model_slot;campaign_clutter_model_count=1;
    model.collision.parts=&part;model.collision.part_count=1;owner.attachment.model=(uint32_t)(uintptr_t)&shared;
    rf_object_registry_init(&campaign_registry);CHECK(!rf_object_registry_insert(&campaign_registry,&owner.state,&owner.state.handle));
    status=scene_clutter_projectile_compose(UINT32_MAX,start,delta,.25f,&hit,&liquid,&matched);CHECK(!status&&matched&&!liquid);
    CHECK(hit.object==SCENE_CLUTTER_PROJECTILE_OWNER&&hit.face==owner.state.handle);
    CHECK(fabsf(hit.hit.fraction-.475f)<.001f&&fabsf(hit.hit.point[2]-5)<.001f&&hit.hit.normal[2]<-.99f);
    hit.hit.fraction=.2f;hit.object=UINT32_MAX;liquid=0;matched=1;
    CHECK(!scene_clutter_projectile_compose(UINT32_MAX,start,delta,.25f,&hit,&liquid,&matched));CHECK(hit.object==UINT32_MAX&&hit.hit.fraction==.2f);
    matched=0;owner.state.flags=0x4000;
    CHECK(!scene_clutter_projectile_compose(UINT32_MAX,start,delta,.25f,&hit,&liquid,&matched)&&!matched);
    owner.state.flags=0;CHECK(!scene_clutter_projectile_compose(owner.state.handle,start,delta,.25f,&hit,&liquid,&matched)&&!matched);
    hit.object=SCENE_CLUTTER_PROJECTILE_OWNER;hit.face=owner.state.handle;
    CHECK(!rf_object_registry_remove(&campaign_registry,owner.state.handle));
    CHECK(!scene_clutter_projectile_damage(&hit,40,3,&handled)&&handled);
    puts("PASS real mesh finite-radius contact, world transform, nearer cover, hidden/source exclusion and stale damage");return 0;
}
