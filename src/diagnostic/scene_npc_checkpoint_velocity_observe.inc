#ifndef RF_SCENE_NPC_CHECKPOINT_VELOCITY_OBSERVE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_VELOCITY_OBSERVE_INCLUDED
/* Failure-only raw observation; not a checkpoint owner or admission policy.
 * Called after the existing velocity reject tuple, without recapture, queries,
 * callbacks or owner mutation. Version1 field map is documented separately.
 * validity:1 owner,2 class,4 descriptor,8 model bank,16 seed pose bank,
 * 32 allocated source body,64 persistence row. Missing groups remain zero.
 * The source body and separately owned corpse/model pose are distinct. */
uint32_t rf_scene_npc_checkpoint_velocity_observation[128];
static void scene_npc_checkpoint_velocity_observe(uint32_t index,uint32_t terminal_dead)
{
    uint32_t *w=rf_scene_npc_checkpoint_velocity_observation,valid=0,cls;
    const campaign_npc_body *o;
    w[0]=1;w[1]=128;memcpy(w+3,rf_scene_npc_checkpoint_reject_state,24);w[9]=index;
    if(!campaign_npc_bodies||index>=campaign_npc_body_count)return;
    o=campaign_npc_bodies+index;valid|=1u;
    w[11]=o->registration.handle;w[12]=o->registration.view!=NULL;
    memcpy(w+13,&o->damage.effects.health,4);w[14]=o->object_flags;
    w[15]=o->view.flags_810;w[16]=o->damage.effects.flags_810;w[17]=o->damage.effects.flags_814;
    w[18]=terminal_dead;w[19]=(uint32_t)o->death.action_824;w[20]=o->movement_slot;
    if(o->movement_slot<16){w[21]=campaign_modes[o->movement_slot].index;valid|=4u;}
    if(campaign_seeds.items&&campaign_seeds.classes&&index<campaign_seeds.records.count){
        cls=campaign_seeds.items[index].class_index;
        if(cls<campaign_seeds.class_count){
            w[10]=cls;w[22]=campaign_seeds.classes[cls].physics.use_kind;
            w[23]=campaign_seeds.classes[cls].physics.flags;
            w[24]=campaign_seeds.classes[cls].physics.flags2;valid|=2u;
        }
    }
    w[25]=o->body.state.flags;w[26]=o->body.state.state_124;
    w[27]=o->scripted_physics;w[28]=o->scripted_physics_seen;
    w[29]=o->support.handle;w[30]=(uint32_t)o->support.material;
    w[31]=o->piece_support.tag;w[32]=o->piece_support.piece;w[33]=o->piece_support.serial;
    w[34]=o->piece_reacquire;w[35]=(uint32_t)o->ai_mode.action_280;
    w[36]=o->patrol.enabled;w[37]=o->patrol.reverse;
    w[38]=o->script_move.active;w[39]=o->script_move.origin;w[40]=o->script_move.event;
    w[41]=o->script_move.follow;w[42]=o->script_move.path_index;w[43]=o->script_move.path_mode;
    w[44]=o->script_move.path_reverse;w[45]=o->script_move.path.count;
    w[46]=o->script_move.route_index;w[47]=o->script_move.retry;w[48]=o->navigation.retained.count;
    memcpy(w+49,&o->script_move.fall_speed,4);
    memcpy(w+50,o->body.state.position,12);memcpy(w+53,o->body.state.next_position,12);
    memcpy(w+56,o->published,12);memcpy(w+59,o->body.state.velocity,12);
    memcpy(w+62,o->body.state.vector_c8,12);memcpy(w+65,o->body.state.mass_vector_d4,12);
    memcpy(w+68,o->body.state.vector_e0,12);memcpy(w+71,o->body.state.vector_ec,12);
    memcpy(w+74,o->support_velocity,12);memcpy(w+77,o->body.state.vector_138,12);
    memcpy(w+80,&o->body.state.scalar_144,4);w[81]=(uint32_t)o->body.state.reference_15c;
    w[82]=o->body.state.word_164;w[83]=o->body.state.word_168;
    memcpy(w+84,o->command_714,12);memcpy(w+87,o->script_move.target,12);
    w[90]=o->collision_contact.material;w[91]=o->collision_contact.reference;w[92]=o->collision_contact.part;
    memcpy(w+93,&o->collision_contact.inverse_mass,4);memcpy(w+94,o->collision_contact.velocity,12);
    memcpy(w+97,o->collision_contact.point,12);memcpy(w+100,&o->body.state.mass,4);
    w[101]=o->body.allocated_bytes;w[102]=o->body.spheres.count;
    w[103]=(uint32_t)o->view.linked_handle;w[104]=(uint32_t)o->attachment_75c;w[105]=o->view.type;
    if(campaign_model_owners&&index<campaign_model_owner_count){
        w[106]=campaign_model_owners[index].owned!=NULL;w[107]=campaign_model_owners[index].pose!=NULL;valid|=8u;
    }
    if(campaign_poses.items&&index<campaign_poses.count){w[108]=1;valid|=16u;}
    memcpy(w+109,o->look.body_angles,12);memcpy(w+112,o->look.command_708,12);
    memcpy(w+115,&scene_robot_death_epoch,8);
    if(o->persistence_registered&&o->persistence_slot<rf_scene_defeated_actors.count&&
       o->persistence_slot<RF_CAMPAIGN_ACTOR_SLOTS){
        w[117]=rf_scene_defeated_actors.items[o->persistence_slot].retired;valid|=64u;
    }
    w[118]=o->body.spheres.items!=NULL;w[119]=o->body.spheres.allocated_bytes;w[120]=o->script_move.stop;
    if(o->body.allocated_bytes)valid|=32u;
    w[2]=valid; /* Publish validity last; raw values above are never normalized. */
}
#endif
