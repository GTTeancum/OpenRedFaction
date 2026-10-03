#ifndef RF_SCENE_NPC_ROTATING_SUPPORT_FIXTURE_INCLUDED
#define RF_SCENE_NPC_ROTATING_SUPPORT_FIXTURE_INCLUDED
/* Isolated DEV8/platform2 fixture. No prescribed contacts or movement modes:
 * ordinary mover interval/carry/sweep/ground/fall/landing code supplies all of
 * these observations. Include after NPC ground code; early prototypes allow
 * fresh setup to reset and the seed loader to offset its one explicit rider. */
enum {SCENE_NPC_ROTATING_FIXTURE_ROWS=8,SCENE_NPC_ROTATING_FIXTURE_WORDS=32};
uint32_t rf_scene_npc_rotating_fixture[SCENE_NPC_ROTATING_FIXTURE_ROWS][SCENE_NPC_ROTATING_FIXTURE_WORDS];
static uint32_t scene_npc_rotating_fixture_loss_frame;
static uint32_t scene_npc_rotating_fixture_enabled(void)
{return rf_scene_dev_room_enabled&&rf_scene_dev_npc_enabled==8&&rf_scene_fragment_platform_enabled==2;}
static void scene_npc_rotating_fixture_reset(void)
{
    memset(rf_scene_npc_rotating_fixture,0,sizeof(rf_scene_npc_rotating_fixture));
    scene_npc_rotating_fixture_loss_frame=UINT32_MAX;
}
static void scene_npc_rotating_fixture_seed(rf_level_entity *record)
{
    if(record&&scene_npc_rotating_fixture_enabled()&&record->uid==0x70000001) {
        record->position[0]=8.699f; /* Rising side, .75 from9.449 pivot. */
        record->position[1]=9.f; /* Raised fixture pivot5.55 clears static floor. */
    }
}
static void scene_npc_rotating_fixture_record(uint32_t row,const campaign_npc_body *owner,uint32_t frame)
{
    uint32_t *p=rf_scene_npc_rotating_fixture[row];
    p[0]=1;p[1]=frame;p[2]=owner->registration.handle;p[3]=owner->support.handle;
    p[4]=owner->movement_slot<16?(uint32_t)campaign_modes[owner->movement_slot].index:UINT32_MAX;
    p[5]=(uint32_t)owner->support.material;memcpy(p+6,owner->body.state.position,12);
    memcpy(p+9,owner->body.state.velocity,12);memcpy(p+12,owner->support_velocity,12);
    memcpy(p+15,owner->body.state.vector_138,12); /* rf_collision_contact_read normal */
    p[18]=rf_scene_npc_rotating_support[1];p[19]=rf_scene_npc_support_lifecycle[0];
    p[20]=rf_scene_npc_support_lifecycle[1];p[21]=rf_scene_npc_support_lifecycle[2];
    p[22]=rf_scene_npc_carry_collision[0];p[23]=rf_scene_npc_idle_ground[3];
    memcpy(p+24,rf_scene_npc_rotating_support+4,12);
    memcpy(p+27,&owner->damage.effects.health,4);
    p[28]=owner->body.state.flags;p[29]=campaign_mover_count?campaign_mover_wrappers[0].handle:0;
    p[30]=rf_scene_npc_idle_ground[4];p[31]=rf_scene_npc_support_lifecycle[7];
}
/* Call after this fixture actor's ordinary idle ground step. A falling mode
 * after frame420 is natural support loss; its next falling frame checks the
 * inherited velocity survives refresh, and the first later static support
 * checks normal landing clears it. All rows are one-shot observations. */
static void scene_npc_rotating_fixture_tick(const campaign_npc_body *owner,uint32_t slot,uint32_t frame)
{
    uint32_t mode;
    if(!scene_npc_rotating_fixture_enabled()||!owner||slot>=campaign_seeds.records.count||
       campaign_seeds.records.items[slot].record.uid!=0x70000001||owner->movement_slot>=16)return;
    mode=(uint32_t)campaign_modes[owner->movement_slot].index;
    if(frame==419)scene_npc_rotating_fixture_record(0,owner,frame);
    if(frame==420)scene_npc_rotating_fixture_record(1,owner,frame);
    if(frame==421)scene_npc_rotating_fixture_record(2,owner,frame);
    if(frame==435)scene_npc_rotating_fixture_record(3,owner,frame);
    if(frame>=420&&mode==3&&scene_npc_rotating_fixture_loss_frame==UINT32_MAX) {
        scene_npc_rotating_fixture_loss_frame=frame;scene_npc_rotating_fixture_record(4,owner,frame);
    } else if(scene_npc_rotating_fixture_loss_frame!=UINT32_MAX&&frame>scene_npc_rotating_fixture_loss_frame) {
        if(mode==3&&!rf_scene_npc_rotating_fixture[5][0])scene_npc_rotating_fixture_record(5,owner,frame);
        if(mode!=3&&!owner->support.handle&&!rf_scene_npc_rotating_fixture[6][0])scene_npc_rotating_fixture_record(6,owner,frame);
    }
    /* A600-frame replay presents599 last; its final simulation tick is598. */
    if(frame==598)scene_npc_rotating_fixture_record(7,owner,frame);
}
#endif
