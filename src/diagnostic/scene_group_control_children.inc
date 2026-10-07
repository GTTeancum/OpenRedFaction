#ifndef RF_SCENE_GROUP_CONTROL_CHILDREN_INC
#define RF_SCENE_GROUP_CONTROL_CHILDREN_INC
/* Bounded scene adapter for the first-list children of 46bbe0. Registry
 * ownership stays with the ordinary static-clutter and trigger factories.
 * Include after campaign_clutter_registered / rf_scene_clutter_pose_set.
 * Open after those factories and controller views, then let the common
 * general_handles builder append child_handle() results in authored order.
 * Duplicate memberships are significant; the core accepts at most four.
 *
 * sync() consumes controller pending transforms with force=1. It advances no
 * clock, commits no controller and changes no gameplay state. Call after
 * controller movement, before contacts. For restore use prepare/validate/assign
 * below, with assignment after RFMC/RFVA/RFPC/mission publication. RFMC owns motion, RFPC owns
 * clutter health/flags, and trigger checkpoints own activation/timers.
 * Do not register these adapter poses in campaign_pose_slots: publication is
 * immediate through the real owners, not a second independent commit path.
 */
enum {SCENE_GROUP_CONTROL_MAX=128,SCENE_GROUP_CONTROL_AUDIT_MAX=16,
      SCENE_GROUP_CONTROL_CLUTTER=4,SCENE_GROUP_CONTROL_TRIGGER=5};
typedef struct scene_group_control_child {
    uint32_t uid,kind,handle,source,memberships;
    void *owner;
    rf_group_attached_pose pose;
} scene_group_control_child;
static scene_group_control_child *campaign_group_control_children;
static uint32_t campaign_group_control_child_count;
/* Read-only diagnostic snapshots. No pointers, writes or synthetic stimuli.
 * Summary: owners, memberships, retained bytes, syncs, publications, changed
 * poses, skipped owners, errors, last UID, last status, audit rows, cap.
 * Row (64 words): UID,kind,handle,source,memberships,present,owner flags,
 * health bits(clutter)/activation flags(trigger); base XYZ/basis at8/11,
 * adapter XYZ/basis at20/23, published XYZ/basis at32/35; first parent
 * handle/flags at44/45, first key XYZ at46, pending XYZ at49, axis at52,
 * distance/sign at55/56, controller index at57, skipped at58, status at59.
 * For multiple memberships the first parent is shown; count stays exact.
 * rf_scene_group_control_child_read() also inspects rows beyond the first16.
 */
uint32_t rf_scene_group_control_children[12];
uint32_t rf_scene_group_control_child_rows[SCENE_GROUP_CONTROL_AUDIT_MAX][64];

static int scene_group_control_child_live(const scene_group_control_child *child,
    uint32_t *flags,uint32_t *skip)
{
    void *registered;uint32_t slot,model;int status;
    if(!child || !flags || !skip)return RF_RANGE;
    registered=rf_object_registry_lookup(&campaign_registry,child->handle);
    if(!registered)return RF_NOT_FOUND;
    if(registered!=child->owner)return RF_FORMAT;
    *skip=0;
    if(child->kind==SCENE_GROUP_CONTROL_CLUTTER){
        rf_clutter_base_owner *owner;
        if(child->source>=campaign_clutter_records.count || !campaign_clutter_bodies ||
           !(owner=campaign_clutter_bodies[child->source]))return RF_NOT_FOUND;
        if((void *)&owner->state!=child->owner || owner->uid!=child->uid ||
           campaign_clutter_records.items[child->source].uid!=child->uid)return RF_FORMAT;
        status=campaign_clutter_registered(child->handle,&slot,&model);if(status)return status;
        if(slot!=child->source || !isfinite(owner->state.health))return RF_FORMAT;
        *flags=owner->state.flags;*skip=!!(*flags&2u) || owner->state.health<=0;
        return RF_OK;
    }
    if(child->kind==SCENE_GROUP_CONTROL_TRIGGER){
        const rf_runtime_trigger *owner;
        if(child->source>=campaign_triggers.count || !campaign_triggers.items)return RF_NOT_FOUND;
        owner=campaign_triggers.items+child->source;
        if((const void *)owner!=child->owner || owner->object_kind!=5 ||
           owner->handle!=child->handle || owner->state.handle!=child->handle ||
           !owner->authored || owner->authored->record.uid!=child->uid)return RF_FORMAT;
        *flags=owner->activation.object_flags;
        /* Disabled/hidden triggers still follow their parent. This copies no
         * activation or visibility flags, so ON/UnHide sees the current volume. */
        return RF_OK;
    }
    return RF_FORMAT;
}

/* Resolve a UID only if its one authored owner is from an admitted family.
 * An absent static model stays absent. A stale or ambiguous owner is never
 * replaced by a newly registered object that happens to share its slot. */
static int scene_group_control_child_resolve(uint32_t uid,scene_group_control_child *out)
{
    scene_group_control_child child={0};uint32_t i,hits=0,flags,skip;int status;
    if(!out)return RF_RANGE;
    for(i=0;i<campaign_clutter_records.count;i++)if(campaign_clutter_records.items[i].uid==uid){
        ++hits;child.kind=SCENE_GROUP_CONTROL_CLUTTER;child.source=i;
    }
    for(i=0;i<campaign_triggers.count;i++){
        if(!campaign_triggers.items[i].authored)return RF_FORMAT;
        if(campaign_triggers.items[i].authored->record.uid==uid){
            ++hits;child.kind=SCENE_GROUP_CONTROL_TRIGGER;child.source=i;
        }
    }
    if(!hits)return RF_NOT_FOUND;
    if(hits!=1 || !uid || uid==UINT32_MAX)return RF_FORMAT;
    /* First-list vehicle and other ordinary UID owners must not be shadowed. */
    for(i=0;i<campaign_seeds.records.count;i++)
        if((uint32_t)campaign_seeds.records.items[i].record.uid==uid)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++)
        if(campaign_events.items[i].authored->record.uid==uid)return RF_FORMAT;
    for(i=0;i<campaign_mover_count;i++)if(campaign_mover_objects[i].uid==uid)return RF_FORMAT;
    for(i=0;i<campaign_group_registration.count;i++)
        if(campaign_group_registration.objects[i].uid==uid)return RF_FORMAT;
    child.uid=uid;
    if(child.kind==SCENE_GROUP_CONTROL_CLUTTER){
        const rf_level_clutter *source=campaign_clutter_records.items+child.source;
        rf_clutter_base_owner *owner;
        if(!campaign_clutter_bodies || !(owner=campaign_clutter_bodies[child.source]))return RF_NOT_FOUND;
        child.handle=owner->state.handle;child.owner=&owner->state;
        memcpy(child.pose.base_position,source->position,12);
        memcpy(child.pose.base_matrix,source->matrix,36);
        child.pose.radius=owner->body.state.bounds.radius;
    }else{
        rf_runtime_trigger *owner=campaign_triggers.items+child.source;rf_trigger_volume volume;
        child.handle=owner->handle;child.owner=owner;
        status=rf_trigger_volume_init(&owner->authored->record,&volume);if(status)return status;
        memcpy(child.pose.base_position,volume.center,12);
        /* Disk rows are forward/right/up. The volume reader has already
         * converted box rows to runtime right/up/forward. Sphere rows remain
         * zero exactly as initialized; orientation is unused for that shape. */
        memcpy(child.pose.base_matrix,volume.matrix,36);
    }
    if(child.handle==UINT32_MAX || (child.handle&0xffffu)>=RF_OBJECT_CAPACITY)return RF_FORMAT;
    status=scene_group_control_child_live(&child,&flags,&skip);if(status)return status;
    child.pose.flags=flags;
    memcpy(child.pose.input_matrix,child.pose.base_matrix,36);
    memcpy(child.pose.output_matrix,child.pose.base_matrix,36);
    memcpy(child.pose.pending_matrix,child.pose.base_matrix,36);
    status=rf_group_pose_set_position(&child.pose,child.pose.base_position);if(status)return status;
    *out=child;return RF_OK;
}

static void campaign_group_control_children_close(void)
{
    free(campaign_group_control_children);campaign_group_control_children=NULL;
    campaign_group_control_child_count=0;
    /* Deliberately no registry removal, owner destruction or pose rollback. */
}
static int campaign_group_control_children_open(void)
{
    uint32_t group,ref,capacity;uint64_t refs=0;int status=RF_OK;
    if(campaign_group_control_children || campaign_group_control_child_count ||
       (campaign_group_runtime.count&&!campaign_controller_views))return RF_RANGE;
    memset(rf_scene_group_control_children,0,sizeof(rf_scene_group_control_children));
    memset(rf_scene_group_control_child_rows,0,sizeof(rf_scene_group_control_child_rows));
    rf_scene_group_control_children[11]=SCENE_GROUP_CONTROL_MAX;
    for(group=0;group<campaign_group_runtime.count;group++){
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+group;
        if(entry->kind==RF_GROUP_RUNTIME_EMPTY)continue;
        if(!entry->source || !entry->source->record.key_count || !entry->source->keys ||
           (entry->source->record.ids_count[0]&&!entry->source->ids[0]))return RF_FORMAT;
        refs+=entry->source->record.ids_count[0];
    }
    if(refs>65536)return RF_RANGE;
    capacity=refs>SCENE_GROUP_CONTROL_MAX?SCENE_GROUP_CONTROL_MAX:(uint32_t)refs;
    if(!capacity)return RF_OK;
    campaign_group_control_children=calloc(capacity,sizeof(*campaign_group_control_children));
    if(!campaign_group_control_children)return RF_IO;
    for(group=0;group<campaign_group_runtime.count;group++){
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+group;
        if(entry->kind==RF_GROUP_RUNTIME_EMPTY)continue;
        for(ref=0;ref<entry->source->record.ids_count[0];ref++){
            uint32_t uid=entry->source->ids[0][ref],i,j;scene_group_control_child child,*found;
            for(i=0;i<campaign_group_control_child_count;i++)
                if(campaign_group_control_children[i].uid==uid)break;
            if(i==campaign_group_control_child_count){
                status=scene_group_control_child_resolve(uid,&child);
                if(status==RF_NOT_FOUND){status=RF_OK;continue;}
                if(status)goto failed;
                if(i==capacity){status=RF_RANGE;goto failed;}
                for(j=0;j<i;j++)if(campaign_group_control_children[j].handle==child.handle ||
                    campaign_group_control_children[j].owner==child.owner){status=RF_FORMAT;goto failed;}
                campaign_group_control_children[i]=child;++campaign_group_control_child_count;
            }
            found=campaign_group_control_children+i;
            if(found->memberships==4){status=RF_RANGE;goto failed;}
            ++found->memberships;++rf_scene_group_control_children[1];
        }
    }
    rf_scene_group_control_children[0]=campaign_group_control_child_count;
    rf_scene_group_control_children[2]=capacity*sizeof(*campaign_group_control_children);
    if(!campaign_group_control_child_count){campaign_group_control_children_close();rf_scene_group_control_children[2]=0;}
    return RF_OK;
 failed:
    ++rf_scene_group_control_children[7];rf_scene_group_control_children[9]=(uint32_t)status;
    campaign_group_control_children_close();return status;
}

static int campaign_group_control_child_handle(uint32_t uid,uint32_t *handle)
{
    uint32_t i,flags,skip;int status;if(!handle)return RF_RANGE;
    for(i=0;i<campaign_group_control_child_count;i++)if(campaign_group_control_children[i].uid==uid){
        const scene_group_control_child *child=campaign_group_control_children+i;
        status=scene_group_control_child_live(child,&flags,&skip);if(status)return status;
        *handle=child->handle;return RF_OK;
    }
    return RF_NOT_FOUND;
}

static int scene_group_control_child_binding(const scene_group_control_child *child,uint32_t *first)
{
    uint32_t group,j,list,n=0,parent=UINT32_MAX;
    if(campaign_group_runtime.count&&!campaign_controller_views)return RF_RANGE;
    for(group=0;group<campaign_group_runtime.count;group++){
        const rf_group_controller_view *view=campaign_controller_views+group;
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+group;
        uint32_t expected=0,matched=0;
        if(view->runtime!=&entry->translation || !entry->source || view->first_key!=entry->source->keys ||
           (view->general_count&&!view->general_handles) || (view->mover_count&&!view->mover_handles))return RF_FORMAT;
        if(entry->kind!=RF_GROUP_RUNTIME_EMPTY){
            if(entry->source->record.ids_count[0]&&!entry->source->ids[0])return RF_FORMAT;
            for(j=0;j<entry->source->record.ids_count[0];j++)expected+=entry->source->ids[0][j]==child->uid;
        }
        for(list=0;list<2;list++){
            const uint32_t *handles=list?view->general_handles:view->mover_handles;
            uint32_t count=list?view->general_count:view->mover_count;
            for(j=0;j<count;j++)if(handles[j]==child->handle){
                if(!list)return RF_FORMAT; /* These owners are first-list children. */
                ++matched;
                if(!n)parent=group;
                if(++n>4 || entry->kind==RF_GROUP_RUNTIME_EMPTY)return RF_FORMAT;
            }
        }
        if(matched!=expected)return RF_FORMAT;
    }
    if(n!=child->memberships || !n)return RF_FORMAT;
    if(first)*first=parent;return RF_OK;
}

int rf_scene_group_control_child_read(uint32_t uid,uint32_t row[64])
{
    uint32_t words[64]={0},i,flags=0,skip=0,parent=UINT32_MAX;int status;
    const scene_group_control_child *child;
    if(!row)return RF_RANGE;
    for(i=0;i<campaign_group_control_child_count;i++)if(campaign_group_control_children[i].uid==uid)break;
    if(i==campaign_group_control_child_count)return RF_NOT_FOUND;
    child=campaign_group_control_children+i;
    words[0]=child->uid;words[1]=child->kind;words[2]=child->handle;
    words[3]=child->source;words[4]=child->memberships;words[44]=words[57]=UINT32_MAX;
    memcpy(words+8,child->pose.base_position,12);memcpy(words+11,child->pose.base_matrix,36);
    memcpy(words+20,child->pose.position,12);memcpy(words+23,child->pose.input_matrix,36);
    status=scene_group_control_child_live(child,&flags,&skip);words[6]=flags;words[58]=skip;
    if(!status){
        words[5]=1;
        if(child->kind==SCENE_GROUP_CONTROL_CLUTTER){
            const rf_clutter_base_owner *owner=campaign_clutter_bodies[child->source];
            memcpy(words+7,&owner->state.health,4);
            memcpy(words+32,owner->state.position,12);memcpy(words+35,owner->matrix,36);
        }else{
            const rf_runtime_trigger *owner=campaign_triggers.items+child->source;
            words[7]=owner->state.flags;
            memcpy(words+32,owner->volume.center,12);memcpy(words+35,owner->volume.matrix,36);
        }
        status=scene_group_control_child_binding(child,&parent);
    }
    words[59]=(uint32_t)status;
    if(parent!=UINT32_MAX){
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+parent;
        const rf_group_controller_view *view=campaign_controller_views+parent;
        for(i=0;i<campaign_group_registration.count;i++)
            if(campaign_group_registration.controllers[i].runtime==entry){
                words[44]=campaign_group_registration.controllers[i].handle;break;
            }
        words[45]=view->runtime->motion.flags;words[57]=parent;
        memcpy(words+46,view->first_key->position,12);memcpy(words+49,view->runtime->pending,12);
        memcpy(words+52,view->first_key->orientation[1],12);
        memcpy(words+55,&view->runtime->distance,4);memcpy(words+56,&view->rotation_sign,4);
    }
    memcpy(row,words,sizeof(words));return RF_OK;
}

/* RFPC identity continues to hash immutable authored poses. Its static-pose
 * guard may accept a differing live pose ONLY through this exact adapter.
 * Retired clutter can retain its last published pose without following later
 * motion; no dead mesh is resurrected or granted a new save-state owner. */
static int campaign_group_control_clutter_pose_valid(uint32_t slot)
{
    uint32_t i,flags,skip;rf_group_attached_pose expected;const scene_group_control_child *child;
    const rf_clutter_base_owner *owner;const rf_level_clutter *source;
    for(i=0;i<campaign_group_control_child_count;i++)
        if(campaign_group_control_children[i].kind==SCENE_GROUP_CONTROL_CLUTTER &&
           campaign_group_control_children[i].source==slot)break;
    if(i==campaign_group_control_child_count)return 0;
    child=campaign_group_control_children+i;
    if(scene_group_control_child_live(child,&flags,&skip))return 0;
    owner=campaign_clutter_bodies[slot];source=campaign_clutter_records.items+slot;
    if(memcmp(child->pose.base_position,source->position,12) || memcmp(child->pose.base_matrix,source->matrix,36))return 0;
    expected=child->pose;expected.flags=flags;
    if(!skip && (scene_group_control_child_binding(child,NULL) ||
       rf_group_translation_bind_pose(&expected,child->handle,campaign_controller_views,campaign_group_runtime.count,1.0f/60,1)))return 0;
    return !memcmp(expected.position,owner->state.position,12) &&
        !memcmp(expected.position,owner->query_position,12) &&
        !memcmp(expected.position,owner->body.state.position,12) &&
        !memcmp(expected.input_matrix,owner->matrix,36) &&
        !memcmp(expected.input_matrix,owner->body.state.orientation,36) &&
        !memcmp(expected.input_matrix,owner->body.state.next_orientation,36);
}

static int campaign_group_control_children_sync(void)
{
    uint32_t i;int status=RF_OK;++rf_scene_group_control_children[3];
    for(i=0;i<campaign_group_control_child_count;i++){
        scene_group_control_child *child=campaign_group_control_children+i;
        rf_group_attached_pose next;uint32_t flags,skip;
        rf_scene_group_control_children[8]=child->uid;
        status=scene_group_control_child_live(child,&flags,&skip);
        if(status==RF_NOT_FOUND || (!status&&skip)){
            ++rf_scene_group_control_children[6];status=RF_OK;continue;
        }
        if(status)break;
        status=scene_group_control_child_binding(child,NULL);if(status)break;
        next=child->pose;next.flags=flags;
        status=rf_group_translation_bind_pose(&next,child->handle,campaign_controller_views,
            campaign_group_runtime.count,1.0f/60,1);if(status)break;
        if(child->kind==SCENE_GROUP_CONTROL_CLUTTER){
            status=rf_scene_clutter_pose_set(child->handle,next.position,next.input_matrix);
            if(status==RF_NOT_FOUND){++rf_scene_group_control_children[6];status=RF_OK;continue;}
            if(status)break;
            /* Existing tag attachments consume this pose-dirty bit before
             * room refresh clears it; Mine Light controls own real glows. */
            campaign_clutter_bodies[child->source]->state.flags|=0x04000000u;
        }else{
            rf_runtime_trigger *owner=campaign_triggers.items+child->source;
            /* Preserve shape, radius, sizes, activation/timers, links and flags.
             * The immutable runtime-space base prevents accumulated drift. */
            memcpy(owner->volume.center,next.position,12);
            memcpy(owner->volume.matrix,next.input_matrix,36);
        }
        rf_scene_group_control_children[5]+=!!memcmp(child->pose.position,next.position,12) ||
            !!memcmp(child->pose.input_matrix,next.input_matrix,36);
        child->pose=next;++rf_scene_group_control_children[4];
    }
    if(status)++rf_scene_group_control_children[7];
    rf_scene_group_control_children[9]=(uint32_t)status;
    rf_scene_group_control_children[10]=campaign_group_control_child_count<SCENE_GROUP_CONTROL_AUDIT_MAX?
        campaign_group_control_child_count:SCENE_GROUP_CONTROL_AUDIT_MAX;
    for(i=0;i<rf_scene_group_control_children[10];i++)
        (void)rf_scene_group_control_child_read(campaign_group_control_children[i].uid,rf_scene_group_control_child_rows[i]);
    return status;
}

/* Restoration is prepared against the mover stage before any live write.
 * General lists are still the original control topology; only vehicle handles
 * may be removed/reinserted by RFVA. Clone each staged controller view so its
 * runtime/key/sign remain staged while first-list references remain live.
 * Prepared clutter fields are precisely the pose-only writes made by
 * scene_clutter_pose_publish, never a saved copy of gameplay/class state. */
typedef struct scene_group_control_restore_entry {
    uint32_t uid,kind,handle,source,present,room;
    void *owner;
    rf_group_attached_pose pose;
    float world_tensor[9];rf_physics_bounds bounds;
} scene_group_control_restore_entry;
typedef struct scene_group_control_restore_stage {
    uint32_t count,controller_count,allocated_bytes,peak_bytes;
    const scene_group_control_child *original_children;
    const rf_group_runtime_entry *original_controllers;
    scene_group_control_restore_entry entries[];
} scene_group_control_restore_stage;

static void campaign_group_control_children_close_stage(scene_group_control_restore_stage **out)
{if(out&&*out){free(*out);*out=NULL;}}

static int campaign_group_control_children_prepare(const rf_group_controller_view *staged,
    uint32_t count,const rf_geometry_collision_world *world,uint32_t budget,
    scene_group_control_restore_stage **out)
{
    scene_group_control_restore_stage *stage=NULL;rf_group_controller_view *views=NULL;
    uint64_t retained,peak;uint32_t i;int status=RF_OK;
    if(!out || *out || !world || count!=campaign_group_runtime.count || count>RF_OBJECT_CAPACITY ||
       (count&&(!staged||!campaign_controller_views)) || campaign_group_control_child_count>SCENE_GROUP_CONTROL_MAX)return RF_RANGE;
    retained=sizeof(*stage)+(uint64_t)campaign_group_control_child_count*sizeof(*stage->entries);
    peak=retained+(uint64_t)count*sizeof(*views);
    if(peak>budget || peak>UINT32_MAX)return RF_RANGE;
    stage=calloc(1,(size_t)retained);if(!stage)return RF_IO;
    if(count){views=malloc(count*sizeof(*views));if(!views){status=RF_IO;goto fail;}}
    stage->count=campaign_group_control_child_count;stage->controller_count=count;
    stage->allocated_bytes=(uint32_t)retained;stage->peak_bytes=(uint32_t)peak;
    stage->original_children=campaign_group_control_children;
    stage->original_controllers=campaign_group_runtime.items;
    for(i=0;i<count;i++){
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+i;
        const rf_group_controller_view *live=campaign_controller_views+i;
        if(!staged[i].runtime || !entry->source || staged[i].first_key!=entry->source->keys ||
           staged[i].rotation_sign!=live->rotation_sign ||
           (staged[i].mover_count&&!staged[i].mover_handles)) {status=RF_FORMAT;goto fail;}
        views[i]=staged[i];views[i].general_handles=live->general_handles;views[i].general_count=live->general_count;
    }
    for(i=0;i<stage->count;i++){
        const scene_group_control_child *child=campaign_group_control_children+i;
        scene_group_control_restore_entry *next=stage->entries+i;uint32_t flags,skip;
        next->uid=child->uid;next->kind=child->kind;next->handle=child->handle;
        next->source=child->source;next->owner=child->owner;next->pose=child->pose;
        status=scene_group_control_child_live(child,&flags,&skip);
        if(status==RF_NOT_FOUND){status=RF_OK;continue;}
        if(status)goto fail;
        status=scene_group_control_child_binding(child,NULL);if(status)goto fail;
        /* A retained dead owner may become live when RFPC is restored, so
         * prepare its pose too; assignment consults the restored health/flags. */
        next->pose.flags=flags;
        status=rf_group_translation_bind_pose(&next->pose,child->handle,views,count,1.0f/60,1);if(status)goto fail;
        if(child->kind==SCENE_GROUP_CONTROL_CLUTTER){
            rf_clutter_base_owner candidate=*campaign_clutter_bodies[child->source];rf_collision_room_location room;
            status=rf_geometry_collision_world_locate(world,next->pose.position,&room);if(status)goto fail;
            next->room=room.room==UINT32_MAX?0:room.room+1;
            status=scene_clutter_pose_publish(&candidate,next->pose.position,next->pose.input_matrix,next->room);if(status)goto fail;
            memcpy(next->world_tensor,candidate.body.state.world_tensor,36);next->bounds=candidate.body.state.bounds;
        }
        next->present=1;
    }
    free(views);*out=stage;return RF_OK;
 fail:free(views);free(stage);return status;
}

static int campaign_group_control_children_validate(const scene_group_control_restore_stage *stage)
{
    uint32_t i,flags,skip;int status;
    if(!stage || stage->count!=campaign_group_control_child_count ||
       stage->original_children!=campaign_group_control_children ||
       stage->controller_count!=campaign_group_runtime.count ||
       stage->original_controllers!=campaign_group_runtime.items)return RF_FORMAT;
    for(i=0;i<stage->count;i++){
        const scene_group_control_child *child=campaign_group_control_children+i;
        const scene_group_control_restore_entry *next=stage->entries+i;
        if(next->uid!=child->uid || next->kind!=child->kind || next->handle!=child->handle ||
           next->source!=child->source || next->owner!=child->owner ||
           memcmp(next->pose.base_position,child->pose.base_position,12) ||
           memcmp(next->pose.base_matrix,child->pose.base_matrix,36))return RF_FORMAT;
        status=scene_group_control_child_live(child,&flags,&skip);
        if(!next->present){if(status!=RF_NOT_FOUND)return RF_FORMAT;continue;}
        if(status)return status;
        status=scene_group_control_child_binding(child,NULL);if(status)return status;
    }
    return RF_OK;
}

/* Assignment-only part of scene_clutter_pose_publish. Health, gameplay flags,
 * physics scheduling/velocities, class data, model/sphere owners stay live. */
static void scene_group_control_restore_publish_clutter(rf_clutter_base_owner *owner,
    const scene_group_control_restore_entry *next)
{
    memcpy(owner->state.position,next->pose.position,12);
    memcpy(owner->query_position,next->pose.position,12);
    memcpy(owner->matrix,next->pose.input_matrix,36);owner->state.first_word=next->room;
    owner->state.flags|=0x04000000u; /* Existing tag-child publication on the next ordinary pass. */
    memcpy(owner->body.state.position,next->pose.position,12);
    memcpy(owner->body.state.next_position,next->pose.position,12);
    memcpy(owner->body.state.orientation,next->pose.input_matrix,36);
    memcpy(owner->body.state.next_orientation,next->pose.input_matrix,36);
    memcpy(owner->body.state.world_tensor,next->world_tensor,36);owner->body.state.bounds=next->bounds;
}

/* Candidate-placement hook, called before the transaction on an RFPC-updated
 * COPY of the exact prop. RF_NOT_FOUND means an ordinary stationary prop.
 * This is deliberately distinct from assigning the eventual live owner. */
static int campaign_group_control_restore_prop(const scene_group_control_restore_stage *stage,
    uint32_t source,rf_clutter_base_owner *candidate)
{
    uint32_t i;if(!stage||!candidate)return RF_RANGE;
    for(i=0;i<stage->count;i++){
        const scene_group_control_restore_entry *next=stage->entries+i;
        if(next->kind!=SCENE_GROUP_CONTROL_CLUTTER || next->source!=source)continue;
        if(!next->present || candidate->uid!=next->uid || candidate->state.handle!=next->handle)return RF_FORMAT;
        if(!(candidate->state.flags&2u) && candidate->state.health>0)
            scene_group_control_restore_publish_clutter(candidate,next);
        return RF_OK;
    }
    return RF_NOT_FOUND;
}

/* UID/handle variant for the world's compact copied-prop array. matched is
 * true for an admitted control, including a retired one; callers may then
 * conservatively remove their unchanged-authored-overlap exception. */
static int campaign_group_control_stage_apply_prop(const scene_group_control_restore_stage *stage,
    rf_clutter_base_owner *candidate,uint32_t *matched)
{
    uint32_t i;int status;if(!stage||!candidate||!matched)return RF_RANGE;
    *matched=0;
    for(i=0;i<stage->count;i++){
        const scene_group_control_restore_entry *next=stage->entries+i;
        if(next->kind!=SCENE_GROUP_CONTROL_CLUTTER || next->uid!=candidate->uid)continue;
        status=campaign_group_control_restore_prop(stage,next->source,candidate);if(status)return status;
        *matched=1;return RF_OK;
    }
    return RF_OK;
}

/* After all component validations and RFMC/RFVA/RFPC/mission assignments,
 * ownership must remain stable. No allocation, callback or fallible query.
 * In particular trigger whole-owner mission assignment must precede this. */
static void campaign_group_control_children_assign(const scene_group_control_restore_stage *stage)
{
    uint32_t i;
    for(i=0;i<stage->count;i++){
        const scene_group_control_restore_entry *next=stage->entries+i;
        scene_group_control_child *child=campaign_group_control_children+i;
        if(!next->present)continue;
        if(next->kind==SCENE_GROUP_CONTROL_CLUTTER){
            rf_clutter_base_owner *owner=campaign_clutter_bodies[next->source];
            if((owner->state.flags&2u) || owner->state.health<=0)continue;
            scene_group_control_restore_publish_clutter(owner,next);
        }else{
            rf_runtime_trigger *owner=campaign_triggers.items+next->source;
            memcpy(owner->volume.center,next->pose.position,12);
            memcpy(owner->volume.matrix,next->pose.input_matrix,36);
        }
        child->pose=next->pose;
    }
}
#endif
