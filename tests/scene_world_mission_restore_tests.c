#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_world_mission_restore.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"world mission restore line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static scene_stream stream;
static rf_campaign_goals goals;
static rf_campaign_local_goals local;
static rf_campaign_pickups pickups,startup;
static rf_campaign_triggers triggers;
static unsigned char goal_wire[RF_CAMPAIGN_GOALS_CHECKPOINT_MAX_BYTES];
static unsigned char pickup_wire[RF_CAMPAIGN_PICKUPS_CHECKPOINT_MAX_BYTES],startup_wire[sizeof(pickup_wire)];
static unsigned char trigger_wire[RF_CAMPAIGN_TRIGGERS_CHECKPOINT_MAX_BYTES];
int main(void)
{
    rf_world_checkpoint world={0};scene_world_mission_stage *stage=NULL;
    rf_level_item items[2]={{0}};uint32_t slots[2]={0,1},a,b,n,passed;uint8_t taken[2]={0,0};
    rf_level_owned_trigger authored={0};rf_runtime_trigger trigger={0},captured={0};int32_t remaining;
    strcpy(campaign_current_level,"L1S1.rfl");world.identity[0]=37;strcpy(world.level,campaign_current_level);
    rf_object_registry_init(&campaign_registry);
    CHECK(!rf_campaign_goal_declare(&goals,"Escape",1));CHECK(!rf_campaign_goal_adjust(&goals,"Escape",1));
    CHECK(!rf_campaign_goal_declare(&goals,"Door",0));CHECK(!rf_campaign_goal_adjust(&goals,"Door",1));
    CHECK(!rf_campaign_local_goals_save(&local,"L1S1.rfl",&goals));
    CHECK(!rf_campaign_goals_checkpoint_encode(world.identity,&goals,&local,goal_wire,sizeof(goal_wire),&n));
    world.sections[RF_WORLD_GOALS-1]=(rf_world_checkpoint_slice){goal_wire,n};
    /* Saved order deliberately differs from current runtime order. */
    CHECK(!rf_campaign_pickup_register(&pickups,"l1s1.rfl",200,&b));
    CHECK(!rf_campaign_pickup_register(&pickups,"l1s1.rfl",100,&a));pickups.items[a].retired=1;
    CHECK(!rf_campaign_pickups_checkpoint_encode(world.identity,&pickups,pickup_wire,sizeof(pickup_wire),&n));
    world.sections[RF_WORLD_PICKUPS-1]=(rf_world_checkpoint_slice){pickup_wire,n};
    CHECK(!rf_campaign_pickup_register(&startup,"L1S1.rfl",900,&a));startup.items[a].retired=1;
    CHECK(!rf_campaign_pickups_checkpoint_encode(world.identity,&startup,startup_wire,sizeof(startup_wire),&n));
    world.sections[RF_WORLD_STARTUP-1]=(rf_world_checkpoint_slice){startup_wire,n};
    CHECK(!rf_campaign_trigger_register(&triggers,"l1s1.rfl",300,&a));
    captured.state.flags=64|8;captured.state.count=3;captured.activation.limit=7;captured.activation.object_flags=2;
    captured.state.activation_time_bits=0x3f800000;
    CHECK(!rf_timer_set(&captured.state.deadline,100,300));CHECK(!rf_timer_set(&captured.contact_timer.deadline,100,90));
    CHECK(!rf_runtime_trigger_save(&captured,150,triggers.states+a));triggers.items[a].retired=1;
    CHECK(!rf_campaign_triggers_checkpoint_encode(world.identity,&triggers,trigger_wire,sizeof(trigger_wire),&n));
    world.sections[RF_WORLD_TRIGGER-1]=(rf_world_checkpoint_slice){trigger_wire,n};
    items[0].uid=100;items[1].uid=200;stream.pickups.items=items;stream.pickups.count=2;stream.pickup_slots=slots;stream.pickup_taken=taken;
    authored.record.uid=300;trigger.authored=&authored;trigger.object_kind=5;trigger.volume.radius=2;
    trigger.state.deadline=trigger.contact_timer.deadline=-1;
    CHECK(!rf_object_registry_insert(&campaign_registry,&trigger,&trigger.handle));trigger.state.handle=trigger.handle;
    campaign_triggers.items=&trigger;campaign_triggers.count=1;
    CHECK(!scene_world_mission_prepare(&stream,&world,1000,1024u*1024u,&stage));
    CHECK(stage&&stage->live_pickups[0].next_slot==1&&stage->live_pickups[1].next_slot==0);
    CHECK(stage->live_pickups[0].next_taken==1&&!stage->live_pickups[1].next_taken);
    CHECK(!rf_scene_mission_goals.count&&!campaign_local_goals.count&&!campaign_trigger_history.count&&!campaign_startup_inventory.count);
    CHECK(slots[0]==0&&slots[1]==1&&!taken[0]&&trigger.state.count==0&&trigger.state.deadline==-1);
    CHECK(!scene_world_mission_validate(stage));
    /* A moving live target must fail admission before any publication. */
    trigger.state.count=1;CHECK(scene_world_mission_validate(stage)==RF_FORMAT);trigger.state.count=0;
    taken[0]=1;CHECK(scene_world_mission_validate(stage)==RF_FORMAT);taken[0]=0;
    slots[1]=9;CHECK(scene_world_mission_validate(stage)==RF_FORMAT);slots[1]=1;
    CHECK(!rf_scene_mission_goals.count&&!campaign_trigger_history.count&&trigger.state.deadline==-1);
    CHECK(!scene_world_mission_validate(stage));scene_world_mission_assign(stage);
    CHECK(!rf_campaign_goal_check(&rf_scene_mission_goals,"escape",1,&passed)&&passed);
    CHECK(campaign_local_goals.count==1&&campaign_startup_inventory.count==1&&campaign_startup_inventory.items[0].retired);
    CHECK(slots[0]==1&&slots[1]==0&&taken[0]==1&&taken[1]==0);
    CHECK(rf_scene_campaign_pickups.items[slots[0]].uid==100&&rf_scene_campaign_pickups.items[slots[1]].uid==200);
    CHECK(trigger.state.count==3&&trigger.state.flags==8&&trigger.activation.limit==7&&trigger.activation.object_flags==2);
    CHECK(trigger.authored==&authored&&trigger.volume.radius==2&&rf_object_registry_lookup(&campaign_registry,trigger.handle)==&trigger&&trigger.state.handle==trigger.handle);
    CHECK(!rf_timer_remaining(trigger.state.deadline,1000,&remaining)&&remaining==250);
    CHECK(!rf_timer_remaining(trigger.contact_timer.deadline,1000,&remaining)&&remaining==40);
    CHECK(campaign_trigger_history.count==1&&campaign_trigger_history.states[0].cooldown_remaining==250);
    scene_world_mission_close(&stage);CHECK(!stage);
    puts("PASS actual mission restore: goals, reordered pickup UID bindings, rebased triggers and stale-live rejection");return 0;
}
