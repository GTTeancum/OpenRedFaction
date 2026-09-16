/* Actual506 authored records with explicitly synthetic resident owner resources.
 * Tests the scope gate/registry ownership, not full clutter asset construction. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clutter scope line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    static const char *names[3]={"lantern_box","lantern_ball","chinese_lamp"};
    rf_vpp archive={0};rf_level level;scene_authored_clutter_baseline baseline,before;
    rf_clutter_base_owner *owners;rf_physics_sphere *spheres;unsigned char source[32]={94},wrong[32]={95};
    uint32_t i,j,handle;float health;rf_object_link *link;rf_clutter_base_owner *owner;
    CHECK(argc==2);CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_level_owned_clutter_open(&level,512*1024,&campaign_clutter_records));CHECK(campaign_clutter_records.count==506);
    owners=calloc(506,sizeof(*owners));spheres=calloc(506,sizeof(*spheres));CHECK(owners && spheres);
    campaign_clutter_bodies=calloc(506,sizeof(*campaign_clutter_bodies));campaign_clutter_model_slots=calloc(506,4);
    campaign_clutter_models=calloc(3,sizeof(*campaign_clutter_models));campaign_clutter_shared=calloc(3,sizeof(*campaign_clutter_shared));
    campaign_clutter_classes.items=calloc(3,sizeof(*campaign_clutter_classes.items));
    CHECK(campaign_clutter_bodies && campaign_clutter_model_slots && campaign_clutter_models && campaign_clutter_shared && campaign_clutter_classes.items);
    campaign_clutter_model_count=campaign_clutter_classes.count=3;rf_object_registry_init(&campaign_registry);rf_object_list_init(&campaign_clutter_objects);
    rf_scene_dev_room_enabled=1;strcpy(campaign_current_level,"ctf06.rfl");
    for(j=0;j<3;j++) {
        strcpy(campaign_clutter_models[j].name,names[j]);campaign_clutter_shared[j].filename=campaign_clutter_models[j].name;
        campaign_clutter_shared[j].resource=&campaign_clutter_models[j].resource;
        campaign_clutter_classes.items[j].name=names[j];campaign_clutter_classes.items[j].model=names[j];campaign_clutter_classes.items[j].life=80;
        campaign_clutter_classes.items[j].flags=2;campaign_clutter_classes.items[j].model_kind=1;
    }
    for(i=0;i<506;i++) {
        const rf_level_clutter *r=campaign_clutter_records.items+i;rf_clutter_base_owner *o=owners+i;
        for(j=0;j<3;j++)if(!rf_emitter_name_lookup(names+j,1,r->class_name))break;CHECK(j<3);
        campaign_clutter_bodies[i]=o;campaign_clutter_model_slots[i]=j;campaign_clutter_shared[j].references++;
        o->uid=r->uid;o->allocated_bytes=sizeof(*o);o->body.allocated_bytes=sizeof(o->body)+sizeof(*spheres);
        o->state.class_index=j;o->state.definition=campaign_clutter_classes.items+j;o->state.health=100;
        o->attachment.model=o->state.model=(uint32_t)(uintptr_t)(campaign_clutter_shared+j);
        memcpy(o->state.position,r->position,12);memcpy(o->matrix,r->matrix,36);memcpy(o->body.state.position,r->position,12);
        memcpy(o->body.state.next_position,r->position,12);memcpy(o->body.state.orientation,r->matrix,36);memcpy(o->body.state.next_orientation,r->matrix,36);
        o->body.spheres.items=spheres+i;o->body.spheres.count=1;spheres[i].radius=.5f;
        rf_object_list_append(&campaign_clutter_objects,&o->object_link);CHECK(!rf_object_registry_insert(&campaign_registry,&o->state,&o->state.handle));
    }
    CHECK(sizeof(baseline)==72);CHECK(!scene_authored_clutter_baseline_capture(source,0,&baseline));before=baseline;
    CHECK(!scene_authored_clutter_baseline_check(source,0,&baseline));
    for(i=0;i<506;i++)owners[i].state.flags^=0x05000010u;
    CHECK(!scene_authored_clutter_baseline_check(source,0,&baseline));
    owner=owners+505;health=owner->state.health;owner->state.health--;
    CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)==RF_FORMAT);owner->state.health=health;
#define MUTATE(field,value) do{uint32_t old=(field);(field)=(value);CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)!=RF_OK);(field)=old;}while(0)
    MUTATE(owner->state.flags,owner->state.flags|2);MUTATE(owner->state.flags,owner->state.flags|0x4000);
    MUTATE(owner->state.handle,owner->state.handle^0x400u);MUTATE(owner->state.model,0);
    MUTATE(owner->uid,owner->uid+1);MUTATE(campaign_clutter_shared[0].references,1);
    MUTATE(campaign_clutter_objects.count,505);MUTATE(campaign_npc_body_count,1);MUTATE(campaign_movers.count,1);
    MUTATE(owner->body.state.flags,owner->body.state.flags^0x80000000u);
    owner->body.state.velocity[0]=1;CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)!=RF_OK);owner->body.state.velocity[0]=0;
    spheres[505].radius=1;CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)==RF_FORMAT);spheres[505].radius=.5f;
    campaign_clutter_classes.items[0].timer++;CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)==RF_FORMAT);campaign_clutter_classes.items[0].timer--;
    campaign_clutter_bodies[505]=NULL;CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)==RF_FORMAT);campaign_clutter_bodies[505]=owner;
    link=owner->object_link.previous;owner->object_link.previous=&campaign_clutter_objects.sentinel;
    CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)==RF_FORMAT);owner->object_link.previous=link;
    handle=owner->state.handle;CHECK(!rf_object_registry_remove(&campaign_registry,handle));
    CHECK(scene_authored_clutter_baseline_check(source,0,&baseline)==RF_FORMAT);
    /* Failed capture preserves the immutable baseline rather than blessing the
     * partially retired source. Do not rebuild it after the missing handle. */
    CHECK(scene_authored_clutter_baseline_capture(source,0,&baseline)!=RF_OK && !memcmp(&baseline,&before,sizeof(before)));
    CHECK(scene_authored_clutter_baseline_check(wrong,0,&baseline)==RF_FORMAT);
    CHECK(scene_authored_clutter_baseline_check(source,1,&baseline)!=RF_OK);
    free(campaign_clutter_classes.items);free(campaign_clutter_shared);free(campaign_clutter_models);
    free(campaign_clutter_model_slots);free(campaign_clutter_bodies);free(spheres);free(owners);
    rf_level_owned_clutter_close(&campaign_clutter_records);rf_vpp_close(&archive);
    puts("PASS actual506/three-class scope:72bytes, unchanged bookkeeping, mutations/stale registry/list/model rejection");return 0;
}
