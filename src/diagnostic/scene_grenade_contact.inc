/* Original4c59f0: object contact consumes both grenade modes immediately.
 * Shield handling returns before direct/radial/ordinary impact requests.
 * Scene tags are classified here, outside the reusable flight integrator. */
uint32_t rf_scene_grenade_contacts[8]; /* contacts,shield,blasts,UID,applied,frame,source,kind */
static uint32_t scene_grenade_object_contact(void *context,const rf_weapon_flight_contact *contact)
{
    uint32_t tag=contact->object;(void)context;
    return tag==SCENE_AI_PROJECTILE_PLAYER || tag==SCENE_DRILLER_PROJECTILE_OWNER ||
        tag==SCENE_TURRET_PROJECTILE_OWNER ||
        (tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER ||
        (tag&0xffff0000u)==SCENE_CLUTTER_PROJECTILE_OWNER;
}
static int scene_grenade_contact_damage(scene_stream *stream,uint32_t frame,
    const rf_weapon_flight_contact *contact,uint32_t flags,uint32_t source,
    float damage,uint32_t *exploded)
{
    uint32_t tag=contact->object,bits,entered,handled;float applied=0,seconds=(float)frame/60,origin[3];int status;
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,&feedback};
    rf_damage_request request={damage,source,campaign_primary[5].damage_kind,0,UINT32_MAX,0};
    *exploded=0;++rf_scene_grenade_contacts[0];rf_scene_grenade_contacts[3]=UINT32_MAX;
    rf_scene_grenade_contacts[4]=0;rf_scene_grenade_contacts[5]=frame;
    rf_scene_grenade_contacts[6]=source;rf_scene_grenade_contacts[7]=(uint32_t)request.kind;
    memcpy(&bits,&seconds,4);
    if((tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
        uint32_t index=tag&0xffffu,consumed;campaign_npc_body *owner;
        if(index>=campaign_npc_body_count)return RF_RANGE;owner=campaign_npc_bodies+index;
        rf_scene_grenade_contacts[3]=(uint32_t)campaign_seeds.records.items[index].record.uid;
        /* Grenade instance bit10 admits armor debit; an ordinary grenade is
         * still consumed by the shield, but its adjusted shield amount is0. */
        status=scene_nano_weapon_contact(owner,campaign_grenade_id,flags&0x10u?damage:0,&consumed);
        if(status)return status;if(consumed){++rf_scene_grenade_contacts[1];return RF_OK;}
        /* Location multiplier1 is the current port policy, not recovered mesh scaling. */
        status=rf_scene_npc_damage(owner->registration.handle,&request,1,bits,&effects,&applied);
        if(status)return status;if(feedback.status)return feedback.status;
        if(owner->damage.effects.health<=0){
            status=rf_scene_npc_death_entry(owner->registration.handle,&entered);if(status)return status;
            if(entered){status=combat_death_start(index);if(status)return status;}
        }
    }else if(tag==SCENE_AI_PROJECTILE_PLAYER){
        status=rf_scene_player_damage(campaign_player_object.handle,&request,1,bits,&effects,&applied);if(status)return status;
    }else{
        status=scene_driller_projectile_damage(contact,source,damage,request.kind,frame,&handled,&applied);if(status)return status;
        if(!handled){status=scene_clutter_projectile_damage(contact,damage,request.kind,&handled);if(status)return status;}
        if(!handled)return RF_NOT_FOUND;
    }
    if(feedback.status)return feedback.status;
    memcpy(rf_scene_grenade_contacts+4,&applied,4);
    /* Ordinary object contact requests direct then radial damage, then impact;
     * no terrain-cut request appears in original4c59f0's object branch. */
    for(uint32_t j=0;j<3;j++)origin[j]=contact->hit.point[j]+contact->hit.normal[j]*.01f;
    status=scene_explosion_blast_source(stream,frame,origin,damage,campaign_grenade.damage_radius,source,3);if(status)return status;
    if(campaign_grenade.impact_count!=1)return RF_FORMAT;
    status=scene_impact_start_sized(stream,contact,frame,campaign_grenade.impact_radius[0]);if(status)return status;
    scene_impact_sound(contact->hit.point,frame);
    ++rf_scene_grenade_contacts[2];*exploded=1;return RF_OK;
}
