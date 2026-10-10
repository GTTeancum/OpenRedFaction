#ifndef RF_SCENE_PLAYER_NPC_CONTACT_INC
#define RF_SCENE_PLAYER_NPC_CONTACT_INC
/* Live actor_tick only: bounded one-way grounded player/humanoid contact.
 * Original48be00 eligibility and49ab00 horizontal max-radius shape, with a
 * deliberate port-local normalized sweep fraction for nearest composition.
 * Original49ab00's additional divide-by-length remains untouched in core.
 * No pair pool, reciprocal NPC publication, floor/support, or saved owner.
 */
typedef struct scene_player_npc_contact {
    uint32_t present,player,handle,slot,material;
    float position[3],velocity[3],radius,inverse_mass;
} scene_player_npc_contact;

static uint32_t scene_player_npc_player(const rf_physics_body_state *state,uint32_t handle)
{
    uint32_t mode=rf_scene_actor_landing[1];
    return state && campaign_spawn && !campaign_cutscene_runtime.active &&
        mode<16 && campaign_modes[mode].index==1 &&
        campaign_player_object.handle==handle && campaign_player_object.object_kind==0 &&
        campaign_player_object.view==&campaign_player_view && campaign_player_view.type==0 &&
        (uint32_t)campaign_player_view.handle==handle && (campaign_player_view.flags_7c&8u) &&
        !(campaign_player_view.flags_7c&(2u|0x4000u|0x40000u)) &&
        campaign_player_view.linked_handle==-1 &&
        campaign_player_damage.state.effects.health>0 &&
        !((campaign_player_view.flags_810|campaign_player_damage.state.effects.flags_810)&1u) &&
        (campaign_player_damage.class_flags&0x20000u) && !(state->flags&0x4000u) &&
        scene_actor_body.allocated_bytes &&
        rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view;
}
static const campaign_npc_body *scene_player_npc_owner(uint32_t slot)
{
    const campaign_npc_body *owner;rf_entity_pose *pose=NULL;uint32_t cls,handle;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count || !campaign_seeds.items ||
       !campaign_seeds.records.items || slot>=campaign_seeds.records.count || !campaign_seeds.classes)return NULL;
    owner=campaign_npc_bodies+slot;handle=owner->registration.handle;
    if(owner->registration.view!=&owner->view || owner->registration.object_kind || owner->view.type ||
       (uint32_t)owner->view.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view ||
       !(owner->damage.effects.health>0) ||
       ((owner->view.flags_810|owner->damage.effects.flags_810)&1u) ||
       ((owner->object_flags|owner->view.flags_7c)&(2u|8u|0x4000u|0x40000u|0x20000u)) ||
       owner->view.linked_handle!=-1 || scene_npc_seat_attached(handle) ||
       !owner->body.allocated_bytes || owner->movement_slot>=16 ||
       campaign_modes[owner->movement_slot].index!=1 || (owner->body.state.flags&(0x4000u|0x400u)))return NULL;
    cls=campaign_seeds.items[slot].class_index;
    if(cls>=campaign_seeds.class_count || !(campaign_seeds.classes[cls].physics.flags&0x20000u) ||
       campaign_seeds.classes[cls].physics.use_kind==1)return NULL;
    if(campaign_actor_pose(slot,&pose) || !pose)return NULL;
    return owner;
}
static int scene_player_npc_radius(const rf_physics_spheres *spheres,float *maximum)
{
    uint32_t i,k;float radius=0;
    if(!spheres || !maximum || !spheres->count || spheres->count>8 || !spheres->items)return RF_FORMAT;
    for(i=0;i<spheres->count;i++){
        if(!isfinite(spheres->items[i].radius) || spheres->items[i].radius<=0)return RF_FORMAT;
        for(k=0;k<3;k++)if(!isfinite(spheres->items[i].center[k]))return RF_FORMAT;
        if(spheres->items[i].radius>radius)radius=spheres->items[i].radius;
    }
    *maximum=radius;return RF_OK;
}
static uint32_t scene_player_npc_pair(const rf_physics_body_state *state,const campaign_npc_body *owner,uint32_t slot)
{
    rf_collision_actor_pair_view a={0},b={0};rf_collision_pair_actor_state first={0},second={0};uint32_t flags=0;
    a.object_flags=campaign_player_view.flags_7c;a.body_flags=state->flags;a.extent_180=state->bounds.radius;
    b.object_flags=owner->object_flags;b.body_flags=owner->body.state.flags;
    b.extent_180=owner->body.state.bounds.radius;b.use_kind=campaign_seeds.classes[campaign_seeds.items[slot].class_index].physics.use_kind;
    /* Proven player-first branch returns before weapon/name fields are used
     * on the admitted flags20 path. Do not approximate extent with model78,
     * class navigation radius, or maximum individual sphere radius. */
    if(rf_collision_actor_pair_reject(&a,&b,0,0,&flags) || flags!=0x20u)return 0;
    first.handle=campaign_player_object.handle;first.parent_handle=(uint32_t)campaign_player_view.linked_handle;
    first.object_flags=campaign_player_view.flags_7c;
    second.handle=owner->registration.handle;second.parent_handle=(uint32_t)owner->view.linked_handle;
    second.object_flags=owner->object_flags;
    return rf_collision_pair_response_allowed(&first,&second);
}
/* Called after the existing actor_sweep, with its original full proposal.
 * The player advances before campaign_script_step, so each NPC is frozen at
 * its current COMMITTED position for this query. Never consume its leftover
 * next_position or project velocity into an interval it has not yet simulated.
 * Velocity plus existing support velocity is contact response metadata only.
 * Every pre-existing hit and exact tie wins. Miss/error preserve all outputs.
 */
static int scene_player_npc_compose(const rf_physics_body_state *state,float normal[3],
    float *fraction,uint32_t *sphere,scene_player_npc_contact *receipt)
{
    rf_geometry_body_hit result={0};scene_player_npc_contact next={0};
    float delta[3],source_radius,limit;uint32_t i,k,any=0;int status;
    if(!state || !normal || !fraction || !sphere || !receipt ||
       !isfinite(*fraction) || *fraction<0 || *fraction>1)return RF_RANGE;
    if(!scene_player_npc_player(state,campaign_player_object.handle))return RF_OK;
    if(!isfinite(state->bounds.radius) || state->bounds.radius<=0)return RF_FORMAT;
    status=scene_player_npc_radius(&scene_actor_body.spheres,&source_radius);if(status)return status;
    for(k=0;k<3;k++){
        if(!isfinite(state->position[k]) || !isfinite(state->next_position[k]))return RF_FORMAT;
        delta[k]=state->next_position[k]-state->position[k];if(!isfinite(delta[k]))return RF_FORMAT;
    }
    if((delta[0]==0 && delta[2]==0) || *fraction==0)return RF_OK;
    limit=*fraction;
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *owner=scene_player_npc_owner(i);float target_radius,ray[6],point[3],length,t,combined_radius;
        double magnitude,inverse,norm;uint32_t separated=0;static const float zero[3]={0};
        if(!owner)continue;
        if(!isfinite(owner->body.state.bounds.radius) || owner->body.state.bounds.radius<=0 ||
           !isfinite(owner->body.state.mass) || owner->body.state.mass<=0 || owner->collision_material>=10)return RF_FORMAT;
        if(!scene_player_npc_pair(state,owner,i))continue;
        status=scene_player_npc_radius(&owner->body.spheres,&target_radius);if(status)return status;
        for(k=0;k<3;k++){
            float low,high,other_low,other_high;
            if(!isfinite(owner->body.state.position[k]) || !isfinite(owner->body.state.velocity[k]) ||
               !isfinite(owner->support_velocity[k]))return RF_FORMAT;
            low=(float)((double)fminf(state->position[k],state->next_position[k])-state->bounds.radius);
            high=(float)((double)fmaxf(state->position[k],state->next_position[k])+state->bounds.radius);
            other_low=(float)((double)owner->body.state.position[k]-owner->body.state.bounds.radius);
            other_high=(float)((double)owner->body.state.position[k]+owner->body.state.bounds.radius);
            if(!isfinite(low)||!isfinite(high)||!isfinite(other_low)||!isfinite(other_high))return RF_FORMAT;
            if(!(low<other_high && other_low<high))separated=1;
            ray[k]=(float)((double)owner->body.state.position[k]-state->position[k]);
            ray[k+3]=-delta[k];
        }
        if(separated)continue;
        for(k=0;k<3;k++)if(!isfinite(ray[k]))return RF_FORMAT;
        combined_radius=(float)((double)source_radius+target_radius);
        if(!isfinite(combined_radius))return RF_FORMAT;
        ray[1]=ray[4]=0;
        magnitude=sqrt(((double)ray[3]*ray[3]+(double)ray[4]*ray[4])+(double)ray[5]*ray[5]);
        length=(float)magnitude;if(!isfinite(length))return RF_FORMAT;if(length==0)continue;
        inverse=1.0/magnitude;for(k=0;k<3;k++)ray[k+3]=(float)((double)ray[k+3]*inverse);
        /*508e40 returns geometric [0,1] time. Deliberately do NOT perform
         *49ad33's second division: this port composes world-style sweep times,
         * rather than emulating the original coupled pair/contact scheduler. */
        if(!rf_collision_ray_sphere(ray,length,zero,combined_radius,point,&t))continue;
        if(!isfinite(t))return RF_FORMAT;
        if(t<0 || !(t<limit))continue;
        if(t==0){double dot=(double)ray[5]*point[2]+(double)ray[4]*point[1]+(double)ray[3]*point[0];if(dot>=0)continue;}
        norm=sqrt(((double)point[0]*point[0]+(double)point[1]*point[1])+(double)point[2]*point[2]);
        memset(&result,0,sizeof(result));
        if(norm>0){inverse=1.0/norm;for(k=0;k<3;k++)result.contact.normal[k]=(float)(-(double)point[k]*inverse);}
        else {result.contact.normal[0]=-1;result.contact.normal[1]=result.contact.normal[2]=0;}
        result.contact.fraction=t;result.contact.material=owner->collision_material;
        result.contact.object_id=owner->registration.handle;result.contact.texture=UINT32_MAX;
        result.contact.face_token=UINT32_MAX;result.solid=result.room=result.face=result.sphere=UINT32_MAX;result.hits=1;
        for(k=0;k<3;k++){
            float travel=(float)((double)delta[k]*t),offset=(float)((double)result.contact.normal[k]*source_radius);
            result.contact.point[k]=(float)((double)state->position[k]+travel);
            result.contact.point[k]=(float)((double)result.contact.point[k]-offset);
            result.contact.velocity[k]=(float)((double)owner->body.state.velocity[k]+owner->support_velocity[k]);
            if(!isfinite(result.contact.point[k]) || !isfinite(result.contact.velocity[k]))return RF_FORMAT;
        }
        next.present=1;next.player=campaign_player_object.handle;next.handle=owner->registration.handle;next.slot=i;
        next.material=owner->collision_material;next.radius=target_radius;
        next.inverse_mass=(float)(1.0/owner->body.state.mass);
        if(!isfinite(next.inverse_mass)||next.inverse_mass<=0)return RF_FORMAT;
        memcpy(next.position,owner->body.state.position,12);memcpy(next.velocity,result.contact.velocity,12);
        limit=t;any=1;
    }
    if(any){rf_scene_actor_body_contact=result;memcpy(normal,result.contact.normal,12);
        *fraction=limit;*sphere=UINT32_MAX;*receipt=next;}
    return RF_OK;
}
/* A typed stack receipt selects the original positive-inverse-mass branch
 *49dcf6..49ddef. Never reinterpret body_hit's reserved_20 as a float. This
 * branch corrects only the player; no NPC mutation or contact publication.
 * Requalify full source/target handles and current pose after caller steps.
 * An obsolete receipt is still HANDLED: it must never become static cover.
 * The consumed translation remains conservative; the next pass requeries.
 */
static int scene_player_npc_respond(const scene_player_npc_contact *receipt,
    rf_physics_body_state *state,const float normal[3],const float support[3],float *impact)
{
    const campaign_npc_body *owner;float n[3],radius,velocity[3];uint32_t k;int status;
    if(!receipt || !receipt->present || !state || !normal || !support || !impact)return RF_RANGE;
    *impact=0;
    if(!scene_player_npc_player(state,receipt->player) || !(owner=scene_player_npc_owner(receipt->slot)) ||
       owner->registration.handle!=receipt->handle || owner->collision_material!=receipt->material ||
       memcmp(owner->body.state.position,receipt->position,12) || !scene_player_npc_pair(state,owner,receipt->slot))return RF_OK;
    status=scene_player_npc_radius(&owner->body.spheres,&radius);if(status)return status;
    if(radius!=receipt->radius || !isfinite(owner->body.state.mass) || owner->body.state.mass<=0 ||
       (float)(1.0/owner->body.state.mass)!=receipt->inverse_mass)return RF_OK;
    for(k=0;k<3;k++){
        velocity[k]=(float)((double)owner->body.state.velocity[k]+owner->support_velocity[k]);
        if(!isfinite(velocity[k]))return RF_FORMAT;
    }
    if(memcmp(velocity,receipt->velocity,12))return RF_OK;
    memcpy(n,normal,12);
    return rf_physics_dynamic_contact(state,n,support,receipt->velocity,1,1,0,1,impact);
}
#endif
