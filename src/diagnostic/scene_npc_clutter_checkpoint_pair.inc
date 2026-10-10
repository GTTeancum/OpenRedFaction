#ifndef RF_SCENE_NPC_CLUTTER_CHECKPOINT_PAIR_INCLUDED
#define RF_SCENE_NPC_CLUTTER_CHECKPOINT_PAIR_INCLUDED
/* Include after scene_world_restore_stage. Original48be00's kind0/kind4
 * branch rejects every nonplayer whose immutable class use_kind is not1.
 * An ordinary guard therefore must not gain a new clutter collision merely
 * because restore used the prop's coarse construction sphere. This adapter
 * proves the two existing owners before consulting the shared classifier.
 * Missing proof retains the conservative checkpoint guard. No geometry,
 * tolerance, save format, collision response or live owner is changed. */
typedef struct scene_npc_clutter_checkpoint_pair {
    const scene_world_restore_stage *world;
    rf_collision_pair_class_view actor;
} scene_npc_clutter_checkpoint_pair;

static uint32_t scene_npc_clutter_checkpoint_pair_prepare(const scene_world_restore_stage *world,
    const rf_npc_checkpoint_record *saved,uint32_t slot,const rf_checkpoint_placement *placement,
    scene_npc_clutter_checkpoint_pair *out)
{
    scene_npc_clutter_checkpoint_pair next={0};const campaign_npc_body *owner;uint32_t cls;
    if(!world||!saved||!placement||!out||saved->retired||saved->dead_pose||!(saved->health>0)||
       slot>=campaign_npc_body_count||!campaign_npc_bodies||!campaign_seeds.items||
       !campaign_seeds.records.items||slot>=campaign_seeds.records.count||!campaign_seeds.classes)return 0;
    owner=campaign_npc_bodies+slot;cls=campaign_seeds.items[slot].class_index;
    if(cls!=saved->class_id||cls>=campaign_seeds.class_count||
       (uint32_t)campaign_seeds.records.items[slot].record.uid!=saved->uid||
       !owner->persistence_registered||!owner->body.allocated_bytes||
       placement->spheres!=owner->body.spheres.items||placement->count!=owner->body.spheres.count||
       memcmp(placement->position,saved->position,12)||owner->registration.object_kind||owner->view.type||
       owner->registration.view!=&owner->view||(uint32_t)owner->view.handle!=owner->registration.handle||
       rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view)return 0;
    next.world=world;next.actor.handle=owner->registration.handle;
    next.actor.actor.object_flags=(owner->object_flags&~0x4004u)|saved->flags;
    next.actor.actor.body_flags=owner->body.state.flags;
    if(saved->physics.present){
        next.actor.actor.object_flags=(next.actor.actor.object_flags&~RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK)|saved->physics.object_bits;
        next.actor.actor.body_flags=(next.actor.actor.body_flags&~RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK)|saved->physics.body_bits;
    }
    next.actor.actor.use_kind=campaign_seeds.classes[cls].physics.use_kind;
    /* Never extend this proof to players, vehicle/use1, corpses or unknown
     * use codes. Movement reconstruction changes body bit1, not the bit20
     * consumed by48be00. Saved hidden/physics bits replace their live values;
     * use_kind!=1 proves rejection independently of the common flag gates. */
    if((next.actor.actor.object_flags&8u)||next.actor.actor.use_kind==1||
       (next.actor.actor.use_kind>6&&next.actor.actor.use_kind!=9&&next.actor.actor.use_kind!=10))return 0;
    *out=next;return 1;
}

static uint32_t scene_npc_clutter_checkpoint_pair_rejected(void *context,uint32_t index)
{
    const scene_npc_clutter_checkpoint_pair *pair=context;const scene_world_restore_stage *world;
    const scene_clutter_restore_entry *entry;const rf_clutter_base_owner *prop,*authored;
    rf_collision_pair_class_view other={0};uint32_t model,flags=0,slot;
    if(!pair||!(world=pair->world)||!world->clutter||index>=world->clutter->count||
       world->world.prop_count!=world->clutter->count||!world->candidate_props||!world->prop_views||
       !campaign_clutter_records.items||!campaign_clutter_bodies||!campaign_clutter_classes.items||
       !campaign_clutter_model_slots||!campaign_clutter_shared||!campaign_clutter_models)return 0;
    entry=world->clutter->entries+index;slot=entry->slot;prop=world->candidate_props+index;
    if(slot>=campaign_clutter_records.count||world->prop_views[index]!=prop||
       !(authored=campaign_clutter_bodies[slot])||prop->uid!=entry->row.uid||
       prop->uid!=campaign_clutter_records.items[slot].uid||prop->uid!=authored->uid||
       prop->state.handle!=entry->handle||prop->state.handle!=authored->state.handle||
       !scene_clutter_damage_identity(&campaign_registry,authored)||
       prop->state.class_index<0||(uint32_t)prop->state.class_index>=campaign_clutter_classes.count||
       (uint32_t)prop->state.class_index!=entry->row.class_id||prop->state.class_index!=authored->state.class_index||
       prop->state.definition!=campaign_clutter_classes.items+prop->state.class_index||
       prop->state.definition!=authored->state.definition||prop->state.definition->model_kind!=1||
       prop->body.spheres.items!=authored->body.spheres.items||prop->body.spheres.count!=authored->body.spheres.count||
       !isfinite(prop->body.state.bounds.radius)||prop->body.state.bounds.radius<0)return 0;
    model=campaign_clutter_model_slots[slot];
    if(model>=campaign_clutter_model_count||
       prop->attachment.model!=(uint32_t)(uintptr_t)(campaign_clutter_shared+model)||
       prop->state.model!=prop->attachment.model||prop->attachment.model!=authored->attachment.model||
       campaign_clutter_shared[model].resource!=&campaign_clutter_models[model].resource)return 0;
    other.kind=4;other.handle=prop->state.handle;
    other.actor.object_flags=prop->state.flags;other.actor.body_flags=prop->body.state.flags;
    other.actor.extent_180=prop->body.state.bounds.radius;
    /* The proof above excludes player bit8, so case4 never reads prop parent.
     * No item/projectile/trigger fields or external predicates are consumed. */
    return rf_collision_pair_reject(&pair->actor,&other,0,0,0,0,0,&flags)==1;
}
#endif
