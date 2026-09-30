#ifndef RF_SCENE_TURRET_GENERATED_REMOVE_FIXTURE_INCLUDED
#define RF_SCENE_TURRET_GENERATED_REMOVE_FIXTURE_INCLUDED
/* Diagnostic runtime-link request, not a fabricated authored Head. Existing
 * Remove_Object owns the gameplay retirement. A temporary registered type2
 * event exercises the same dispatcher/callback with a generation-checked head.
 * It is synchronous and removed before this function returns. */
uint32_t rf_scene_turret_head_remove_uid;
/* UID,dispatches,basehandle,headhandle,base registered,head registered,
 * base health,head health,baseflags,headflags,headlink,headdead,retirementreason,
 * base persistence retired,events,unsupported,other targets,status,frame,
 * shots at removal,temporary event removed,base ready,base action,head target */
uint32_t rf_scene_turret_head_remove_test[24];
static int scene_turret_generated_remove_fixture(uint32_t frame)
{
    scene_turret_generated_key key={rf_scene_turret_head_remove_uid,SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD};
    scene_turret_generated_binding *b;scene_turret_owner *head;campaign_npc_body *base;
    uint32_t handle,reason;int status;
    if(!rf_scene_turret_head_remove_uid)return RF_OK;
    if(!frame)memset(rf_scene_turret_head_remove_test,0,sizeof(rf_scene_turret_head_remove_test));
    if(frame<60)return RF_OK;
    status=scene_turret_generated_resolve(key,&handle);if(status)goto fail;
    b=scene_turret_generated_find(handle);head=scene_turret_lookup(handle);
    base=b?scene_turret_generated_base(b):NULL;
    if(!base||!head||!base->persistence_registered||base->persistence_slot>=rf_scene_defeated_actors.count){status=RF_NOT_FOUND;goto fail;}
    if(!rf_scene_turret_head_remove_test[1]) {
        rf_runtime_event event={0};rf_level_owned_event authored={0};
        rf_level_link_target link={handle,1,0};rf_startup_events_report report={0};
        int removed;
        if(frame!=60||head->dead||head->damage.effects.health<=0||base->damage.effects.health<=0){status=RF_FORMAT;goto fail;}
        event.object_kind=6;event.authored=&authored;event.links=&link;event.state.type=2;event.state.deadline=-1;
        authored.record.link_count=1;
        status=rf_object_registry_insert(&campaign_registry,&event,&event.handle);if(status)goto fail;
        status=rf_runtime_event_fire(&campaign_triggers,event.handle,UINT32_MAX,UINT32_MAX,
            (int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD),&scene_gravity,NULL,NULL,&report);
        removed=rf_object_registry_remove(&campaign_registry,event.handle);
        rf_scene_turret_head_remove_test[20]=!removed;
        if(!status)status=removed;
        ++rf_scene_turret_head_remove_test[1];
        rf_scene_turret_head_remove_test[14]=report.events;
        rf_scene_turret_head_remove_test[15]=report.unsupported_actions;
        rf_scene_turret_head_remove_test[16]=report.other_targets;
        rf_scene_turret_head_remove_test[19]=rf_scene_turret_shots[0];
        if(status)goto fail;
        if(report.events!=1||report.unsupported_actions||report.other_targets||report.unresolved_targets){status=RF_FORMAT;goto fail;}
    }
    status=scene_turret_generated_retirement_read((uint32_t)(b-scene_turret_generated),&reason);if(status)goto fail;
    rf_scene_turret_head_remove_test[0]=key.base_uid;
    rf_scene_turret_head_remove_test[2]=b->base_handle;rf_scene_turret_head_remove_test[3]=handle;
    rf_scene_turret_head_remove_test[4]=scene_turret_generated_base(b)!=NULL;
    rf_scene_turret_head_remove_test[5]=scene_turret_lookup(handle)!=NULL;
    memcpy(rf_scene_turret_head_remove_test+6,&base->damage.effects.health,4);
    memcpy(rf_scene_turret_head_remove_test+7,&head->damage.effects.health,4);
    rf_scene_turret_head_remove_test[8]=base->object_flags;
    rf_scene_turret_head_remove_test[9]=head->view.flags_7c;
    rf_scene_turret_head_remove_test[10]=(uint32_t)head->view.linked_handle;
    rf_scene_turret_head_remove_test[11]=head->dead;rf_scene_turret_head_remove_test[12]=reason;
    rf_scene_turret_head_remove_test[13]=rf_scene_defeated_actors.items[base->persistence_slot].retired;
    rf_scene_turret_head_remove_test[18]=frame;
    rf_scene_turret_head_remove_test[21]=base->view.flags_7d0&1u;
    rf_scene_turret_head_remove_test[22]=(uint32_t)base->view.action_520;
    rf_scene_turret_head_remove_test[23]=head->target;return RF_OK;
fail:rf_scene_turret_head_remove_test[17]=(uint32_t)status;return status;
}
#endif
