/* Profile2 excludes mutable props from the save. Capture once after fresh
 * ctf06 owner/glare construction, before gameplay; never recapture to forgive
 * mutation. This72-byte process-local baseline is NOT serialized identity.
 * Include after checkpoint_sha helpers and campaign clutter globals/functions.
 */
typedef struct scene_authored_clutter_baseline {
    unsigned char source[32],digest[32];uint32_t count,ready;
} scene_authored_clutter_baseline;
static int scene_authored_clutter_fingerprint(const unsigned char source[32],uint32_t vehicle_count,unsigned char out[32])
{
    static const char *const names[3]={"lantern_box","lantern_ball","chinese_lamp"};
    static const uint32_t expected[3]={316,106,84};
    uint32_t counts[3]={0},references[3]={0},i,j,k,m;checkpoint_sha hash;
    if(!source || !out || !rf_scene_dev_room_enabled || strcmp(campaign_current_level,"ctf06.rfl") ||
       campaign_npc_body_count || campaign_movers.count || vehicle_count ||
       campaign_clutter_records.count!=506 || campaign_clutter_model_count!=3 ||
       campaign_clutter_objects.count!=506 || !campaign_clutter_records.items || !campaign_clutter_bodies ||
       !campaign_clutter_model_slots || !campaign_clutter_models || !campaign_clutter_shared ||
       !campaign_clutter_classes.items)return RF_RANGE;
    checkpoint_sha_init(&hash);checkpoint_sha_add(&hash,"RFCP2 immutable clutter v1",26);checkpoint_sha_add(&hash,source,32);
#define WORD(v) checkpoint_sha_word(&hash,(uint32_t)(v))
#define FLOAT(v) do{float value=(v);if(!isfinite(value))return RF_FORMAT;checkpoint_sha_float(&hash,value);}while(0)
#define VECTOR(v,n) do{for(k=0;k<(n);k++)FLOAT((v)[k]);}while(0)
    for(i=0;i<506;i++) {
        const rf_level_clutter *r=campaign_clutter_records.items+i;
        const rf_clutter_base_owner *o=campaign_clutter_bodies[i];const rf_clutter_state *state;
        const rf_physics_body_state *body;const rf_clutter_class *cls;
        if(!o || !r->class_name || !r->name || !r->resource_name)return RF_FORMAT;
        state=&o->state;body=&o->body.state;m=campaign_clutter_model_slots[i];
        if(m>=3 || rf_object_registry_lookup(&campaign_registry,state->handle)!=state ||
           state->class_index<0 || (uint32_t)state->class_index>=campaign_clutter_classes.count ||
           state->definition!=campaign_clutter_classes.items+state->class_index || o->uid!=r->uid ||
           o->attachment.model!=(uint32_t)(uintptr_t)(campaign_clutter_shared+m) || state->model!=o->attachment.model ||
           campaign_clutter_shared[m].resource!=&campaign_clutter_models[m].resource ||
           !campaign_clutter_shared[m].filename || strcmp(campaign_clutter_shared[m].filename,campaign_clutter_models[m].name) ||
           !o->allocated_bytes || !o->body.allocated_bytes || !o->body.spheres.items || o->body.spheres.count!=1 ||
           memcmp(state->position,r->position,12) || memcmp(o->matrix,r->matrix,36) ||
           (state->flags&(2u|0x4000u)) || state->health<=0)return RF_FORMAT;
        cls=state->definition;if(!cls->name || !cls->model || cls->flags!=2 || cls->model_kind!=1 || cls->life!=80 ||
           rf_emitter_name_lookup(&cls->name,1,r->class_name)!=0)return RF_FORMAT;
        for(j=0;j<3;j++)if(rf_emitter_name_lookup(names+j,1,r->class_name)==0)break;
        if(j==3)return RF_FORMAT;counts[j]++;references[m]++;
        /* Validate complete authored object-list ownership, not just count. */
        if(o->object_link.previous!=(i?&campaign_clutter_bodies[i-1]->object_link:&campaign_clutter_objects.sentinel) ||
           o->object_link.next!=(i+1<506?(campaign_clutter_bodies[i+1]?&campaign_clutter_bodies[i+1]->object_link:NULL):&campaign_clutter_objects.sentinel))return RF_FORMAT;
        WORD(r->uid);WORD(r->enabled);checkpoint_sha_add(&hash,r->class_name,(uint32_t)strlen(r->class_name)+1);
        checkpoint_sha_add(&hash,r->name,(uint32_t)strlen(r->name)+1);checkpoint_sha_add(&hash,r->resource_name,(uint32_t)strlen(r->resource_name)+1);
        checkpoint_sha_add(&hash,cls->model,(uint32_t)strlen(cls->model)+1);
        WORD(cls->flags);WORD(cls->timer);FLOAT(cls->life);FLOAT(cls->radius);WORD(cls->material);WORD(cls->model_kind);
        WORD(state->handle);WORD(state->first_word);WORD(state->flags&~0x05000010u);WORD(state->physics_flags);
        /* Only visited01000000, room-dirty04000000 and rendered10 are excluded:
         * scene attachments pass and rf_object_render_dispatch update these. */
        VECTOR(state->position,3);FLOAT(state->health);FLOAT(state->armor);WORD(state->corpse);WORD(state->skin);
        WORD(state->timer_a4);WORD(state->word_a8);WORD(state->timer_b0);WORD(state->timer_b4);WORD(state->emitter_head);WORD(state->word_b8);
        WORD(state->sound);WORD(state->sound_d0);WORD(state->byte_cc);WORD(state->slot);
        WORD(o->material);WORD(o->parent_group);WORD(o->parent_byte);WORD(o->identifier);VECTOR(o->matrix,9);
        FLOAT(o->attachment.radius);WORD(o->attachment.model_index);WORD(o->attachment.model_property);
        WORD(body->flags);WORD(body->state_124);VECTOR(body->position,3);VECTOR(body->next_position,3);
        VECTOR(body->orientation,9);VECTOR(body->next_orientation,9);VECTOR(body->velocity,3);
        for(j=0;j<3;j++)if(body->velocity[j]!=0)return RF_FORMAT;
        VECTOR(body->coefficients,3);FLOAT(body->mass);VECTOR(body->local_tensor,9);VECTOR(body->world_tensor,9);
        VECTOR(body->vector_c8,3);VECTOR(body->mass_vector_d4,3);VECTOR(body->vector_e0,3);VECTOR(body->vector_ec,3);
        VECTOR(body->bounds.minimum,3);VECTOR(body->bounds.maximum,3);FLOAT(body->bounds.radius);
        VECTOR(body->vector_138,3);FLOAT(body->scalar_144);WORD(body->reference_15c);WORD(body->word_164);WORD(body->word_168);
        VECTOR(o->body.spheres.items[0].center,3);FLOAT(o->body.spheres.items[0].radius);
        FLOAT(o->body.spheres.items[0].parameter_10);WORD(o->body.spheres.items[0].opaque_14);
    }
    if(campaign_clutter_objects.sentinel.next!=&campaign_clutter_bodies[0]->object_link ||
       campaign_clutter_objects.sentinel.previous!=&campaign_clutter_bodies[505]->object_link)return RF_FORMAT;
    for(j=0;j<3;j++)if(counts[j]!=expected[j] || !references[j] || campaign_clutter_shared[j].references!=references[j])return RF_FORMAT;
    checkpoint_sha_end(&hash,out);return RF_OK;
#undef WORD
#undef FLOAT
#undef VECTOR
}
static int scene_authored_clutter_baseline_capture(const unsigned char source[32],uint32_t vehicle_count,
    scene_authored_clutter_baseline *out)
{
    scene_authored_clutter_baseline value={0};int status;if(!out)return RF_RANGE;
    status=scene_authored_clutter_fingerprint(source,vehicle_count,value.digest);if(status)return status;
    memcpy(value.source,source,32);value.count=506;value.ready=1;*out=value;return RF_OK;
}
static int scene_authored_clutter_baseline_check(const unsigned char source[32],uint32_t vehicle_count,
    const scene_authored_clutter_baseline *baseline)
{
    unsigned char digest[32];int status;
    if(!source || !baseline || baseline->ready!=1 || baseline->count!=506 || memcmp(source,baseline->source,32))return RF_FORMAT;
    status=scene_authored_clutter_fingerprint(source,vehicle_count,digest);if(status)return status;
    return memcmp(digest,baseline->digest,32)?RF_FORMAT:RF_OK;
}
