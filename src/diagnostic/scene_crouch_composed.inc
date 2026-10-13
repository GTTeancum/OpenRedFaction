#ifndef RF_SCENE_CROUCH_COMPOSED_INCLUDED
#define RF_SCENE_CROUCH_COMPOSED_INCLUDED
/* Incoming physical shape has no combat-target or living-player requirement.
 * Resolve RFNS attachment independently of the old destination link. */
static int scene_crouch_composed(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player)
{
    if(!stage||!player||player->npcs!=stage)return RF_RANGE;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        const campaign_npc_body *o;uint32_t seated;float basis[9];int status;
        status=scene_crouch_prepared(stage,e);if(status)return status;
        if(!e->crouch_shape_present)continue;
        o=campaign_npc_bodies+e->slot;
        if(!e->registered||(e->saved.retired&&!e->crouch_retire_pending)||scene_npc_reload_identity(e->slot,o)||
           e->handle==campaign_player_object.handle||(o->object_flags&8u)||
           o->view.occupant_count||o->attachment_75c!=-1||
           (o->view.linked_handle!=-1&&!scene_npc_seat_checkpoint_actor(e->handle)))return RF_FORMAT;
        status=scene_npc_seat_checkpoint_basis(seats,&e->saved,basis,&seated);if(status)return status;
        if(seated)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_crouch_composed_prepare(scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player)
{
    int status=scene_crouch_composed(stage,seats,player);if(status)return status;
    stage->crouch_joined=1;return RF_OK;
}
#endif
