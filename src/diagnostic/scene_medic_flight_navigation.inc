#ifndef RF_SCENE_MEDIC_FLIGHT_NAVIGATION_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_NAVIGATION_INCLUDED
/* Private, source-qualified action7 projection. Include after actual scene
 * graph/NPC/model owners. No AI transition, RNG, playback or physical step.
 * Source40abd0/40aae0/4ceee0,40b6d0/40bb70; see lane evidence receipt. */
static uint32_t scene_flight_finite3(const float v[3])
{return isfinite(v[0])&&isfinite(v[1])&&isfinite(v[2]);}
static double scene_flight_length(const float v[3])
{return sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);}
static double scene_flight_distance(const float a[3],const float b[3])
{
    float v[3];for(uint32_t i=0;i<3;i++)v[i]=(float)((double)a[i]-b[i]);
    return scene_flight_length(v);
}
static double scene_flight_distance_squared(const float a[3],const float b[3])
{
    float v[3];for(uint32_t i=0;i<3;i++)v[i]=(float)((double)a[i]-b[i]);
    return ((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2];
}
static uint32_t scene_flight_owner_live(const campaign_npc_body *o)
{
    return o&&o->registration.view==&o->view&&o->view.type==0&&
        rf_object_registry_lookup(&campaign_registry,o->registration.handle)==&o->registration&&
        rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view;
}
static uint32_t scene_flight_threat_live(const scene_medic_flight_threat_view *t)
{
    /* A dead registered player still resolves. Health is deliberately absent. */
    return t&&t->view==&campaign_player_view&&campaign_player_object.view==t->view&&
        campaign_player_object.handle==t->handle&&
        rf_object_registry_lookup(&campaign_registry,t->handle)==&campaign_player_object&&
        rf_entity_lookup(&campaign_entities,(int32_t)t->handle)==t->view;
}
/* Interval-only actual player controller receipt. The successful logical
 * replacement and restore hooks invalidate it with NULL, including in-place
 * form changes that keep the same numeric registry handle. */
static struct {uint32_t valid,handle;const rf_entity_view *view;int32_t current,next;} scene_flight_player_motion;
static void scene_medic_flight_player_motion(const rf_motion_controller *controller)
{
    scene_flight_player_motion.valid=0;
    if(!controller||campaign_player_object.view!=&campaign_player_view||
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)!=&campaign_player_object)return;
    scene_flight_player_motion.handle=campaign_player_object.handle;scene_flight_player_motion.view=&campaign_player_view;
    scene_flight_player_motion.current=controller->current;scene_flight_player_motion.next=controller->next;
    scene_flight_player_motion.valid=1;
}
static int scene_flight_node_resolve(const campaign_npc_body *o,scene_medic_flight_node ref,
    rf_entity_navigation_candidate **out)
{
    if(!o||!out)return RF_RANGE;
    if(ref.kind==SCENE_FLIGHT_NODE_NONE){if(ref.ordinal)return RF_FORMAT;*out=NULL;return RF_OK;}
    if(ref.kind==SCENE_FLIGHT_NODE_OWN_START){
        if(ref.ordinal)return RF_FORMAT;
        *out=(rf_entity_navigation_candidate *)&o->navigation.start;return RF_OK;
    }
    if(ref.kind!=SCENE_FLIGHT_NODE_AUTHORED||ref.ordinal>=campaign_navigation.count||
       !campaign_navigation.nodes||!campaign_navigation.references)return RF_FORMAT;
    if(campaign_navigation.references[ref.ordinal].candidate!=&campaign_navigation.nodes[ref.ordinal].candidate)return RF_FORMAT;
    *out=campaign_navigation.references[ref.ordinal].candidate;return RF_OK;
}
static int scene_flight_node_capture(const campaign_npc_body *o,
    const rf_entity_navigation_candidate *node,scene_medic_flight_node *out)
{
    if(!o||!out)return RF_RANGE;
    if(!node){*out=(scene_medic_flight_node){SCENE_FLIGHT_NODE_NONE,0};return RF_OK;}
    if(node==&o->navigation.start){*out=(scene_medic_flight_node){SCENE_FLIGHT_NODE_OWN_START,0};return RF_OK;}
    if(campaign_navigation.count&&(!campaign_navigation.nodes||!campaign_navigation.references))return RF_FORMAT;
    for(uint32_t i=0;i<campaign_navigation.count;i++)if(node==&campaign_navigation.nodes[i].candidate){
        if(campaign_navigation.references[i].candidate!=node)return RF_FORMAT;
        *out=(scene_medic_flight_node){SCENE_FLIGHT_NODE_AUTHORED,i};return RF_OK;
    }
    return RF_FORMAT; /* An ordinary own-goal/scratch pointer is never admitted. */
}
static void scene_flight_nav_clear(scene_medic_flight_nav *n,int32_t now)
{
    /*40a950 projection: pair, request-eye, bias, start and steering survive. */
    n->count=0;n->previous=n->current=-1;n->wait_134=now;
    memset(n->prefix,0,sizeof(n->prefix));memset(n->turn_138,0,sizeof(n->turn_138));
    n->approach_15c=1;n->activity_170=0;
}
static int scene_medic_flight_nav_admit(const campaign_npc_body *o,
    const scene_medic_flight_nav *n,const scene_medic_flight_start *start)
{
    uint32_t own=0;rf_entity_navigation_candidate *node;int status;
    if(!o||!n||!start)return RF_RANGE;
    if(n->count>4||n->count==1||n->approach_15c>1||n->activity_170>1||
       n->start_valid>1||n->eye_valid>1||n->wait_134<0||n->wait_134>RF_TIMER_PERIOD||
       (n->bias_158!=-.5f&&n->bias_158!=0&&n->bias_158!=.5f)||
       !scene_flight_finite3(n->turn_138)||!scene_flight_finite3(n->steering_174)||
       !scene_flight_finite3(n->request_eye_14c))return RF_FORMAT;
    if(n->count){
        if(n->previous<0||n->current<0||(uint32_t)n->previous>=n->count||
           (uint32_t)n->current>=n->count||!n->start_valid||!n->eye_valid)return RF_FORMAT;
    }else if(n->previous!=-1||n->current!=-1||n->activity_170)return RF_FORMAT;
    if(!n->eye_valid&&(n->request_eye_14c[0]!=0||n->request_eye_14c[1]!=0||n->request_eye_14c[2]!=0))return RF_FORMAT;
    for(uint32_t i=0;i<6;i++){
        scene_medic_flight_node ref=i<2?n->pair[i]:n->prefix[i-2];
        if(i>=2&&i-2>=n->count&&(ref.kind||ref.ordinal))return RF_FORMAT;
        status=scene_flight_node_resolve(o,ref,&node);if(status)return status;
        if(i>=2&&i-2<n->count&&!node)return RF_FORMAT;
        own|=ref.kind==SCENE_FLIGHT_NODE_OWN_START;
        if(node&&ref.kind==SCENE_FLIGHT_NODE_AUTHORED&&
           (!scene_flight_finite3(node->position)||!isfinite(node->radius)||node->radius<0||
            !isfinite(node->height)||node->height<0))return RF_FORMAT;
    }
    if(own&&!n->start_valid)return RF_FORMAT;
    if(!n->start_valid){
        const scene_medic_flight_start zero={0};
        if(memcmp(start,&zero,sizeof(zero)))return RF_FORMAT;
    }else if(!scene_flight_finite3(start->position)||!scene_flight_finite3(start->query_point)||
        !isfinite(start->radius)||start->radius<0||!isfinite(start->height)||start->height<0||
        start->radius!=o->navigation_radius||start->height!=o->death_item_extent_7c4||start->word_040)return RF_FORMAT;
    return RF_OK;
}
static int scene_medic_flight_nav_capture(const campaign_npc_body *o,
    scene_medic_flight_nav *nav,scene_medic_flight_start *start)
{
    scene_medic_flight_start value={0};int status;
    if(!o||!nav||!start)return RF_RANGE;
    if(o->flight.nav.start_valid){
        memcpy(value.position,o->navigation.start.position,12);memcpy(value.query_point,o->navigation.start.query_point,12);
        value.radius=o->navigation.start.radius;value.height=o->navigation.start.height;value.word_040=o->navigation.start.word_040;
    }
    status=scene_medic_flight_nav_admit(o,&o->flight.nav,&value);if(status)return status;
    *nav=o->flight.nav;*start=value;return RF_OK;
}
static void scene_medic_flight_nav_publish(campaign_npc_body *o,
    const scene_medic_flight_nav *nav,const scene_medic_flight_start *start)
{
    /* Admission is complete. The only live endpoint is this existing member.
     * No allocation, graph search or callbacks may occur at publication. */
    o->flight.nav=*nav;
    if(nav->start_valid){
        memcpy(o->navigation.start.position,start->position,12);memcpy(o->navigation.start.query_point,start->query_point,12);
        o->navigation.start.radius=start->radius;o->navigation.start.height=start->height;o->navigation.start.word_040=start->word_040;
    }
}
typedef struct scene_flight_search {
    rf_collision_solid_view solid;const scene_medic_flight_threat_view *threat;
    float radius,height;
} scene_flight_search;
static int scene_flight_nearest_edge(void *context,uint32_t token,const float a[3],const float b[3],float threshold,uint32_t *result)
{
    scene_flight_search *c=context;
    if(token!=c->threat->handle||!scene_flight_threat_live(c->threat))return RF_NOT_FOUND;
    return rf_entity_navigation_edge_allowed(c->threat->position,a,b,threshold,result);
}
static int scene_flight_nearest_visible(void *context,rf_entity_navigation_candidate *node,
    const float point[3],float radius,float height,uint32_t *result)
{
    scene_flight_search *c=context;
    return rf_entity_navigation_visible_solid(&c->solid,node,point,radius,height,result);
}
static int scene_flight_nearest(void *context,const float point[3],uint32_t alternate,uint32_t *token)
{
    scene_flight_search *c=context;
    rf_entity_navigation_nearest_backend b={scene_flight_nearest_edge,scene_flight_nearest_visible,c};
    return rf_entity_navigation_nearest(campaign_navigation_workspace.references,campaign_navigation_workspace.count,
        point,c->radius,c->height,alternate,4.0f,&b,token);
}
static uint32_t scene_flight_grounded_mode(int32_t kind)
{return !(kind==12||kind==15||kind==13||kind==11||kind==9||kind==4||kind==7);}
static int scene_medic_flight_nav_request(scene_stream *stream,campaign_npc_body *o,
    const scene_medic_flight_threat_view *threat,int32_t now,uint32_t *found)
{
    rf_level_navigation_workspace *w=&campaign_navigation_workspace;scene_flight_search c={0};
    rf_entity_navigation_retained_route route={0};rf_entity_navigation_search_query q={0};
    rf_entity_navigation_reference saved_goal,saved_start;rf_entity_navigation_candidate dummy={0},*node;
    uint32_t links[2],tokens[2],pair[2]={0},value=0,n,mode;rf_entity_navigation_token_list list={tokens,0,2};
    scene_medic_flight_node prefix[4]={{0}};int status;
    if(!stream||!stream->collision||!o||!threat||!found||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!scene_flight_owner_live(o)||!scene_flight_threat_live(threat))return RF_NOT_FOUND;
    if(!scene_flight_finite3(threat->position)||!scene_flight_finite3(o->published)||
       !scene_flight_finite3(o->eye_position)||o->movement_slot>=16||scene_flight_search_busy)return RF_FORMAT;
    if(!isfinite(o->navigation_radius)||o->navigation_radius<0||!isfinite(o->death_item_extent_7c4)||o->death_item_extent_7c4<0)return RF_FORMAT;
    n=w->count;
    if(n!=campaign_navigation.count||n>65534|| (n&&(!w->storage||!w->references||!w->scratch)))return RF_FORMAT;
    /* Keys are an internal search namespace, never serialized ordinals. */
    for(uint32_t i=0;i<n;i++){
        status=scene_flight_node_resolve(o,(scene_medic_flight_node){SCENE_FLIGHT_NODE_AUTHORED,i},&node);
        if(status)return status;
        if(w->references[i].candidate!=node||w->references[i].order_key!=i+1)return RF_FORMAT;
    }
    for(uint32_t i=0;i<2;i++){
        status=scene_flight_node_resolve(o,o->flight.nav.pair[i],&node);if(status)return status;
        if(node){pair[i]=o->flight.nav.pair[i].kind==SCENE_FLIGHT_NODE_OWN_START?n+2:o->flight.nav.pair[i].ordinal+1;}
    }
    /*40abd0 reset happens before40aae0 and an ordinary search miss. */
    scene_flight_nav_clear(&o->flight.nav,now);
    memcpy(o->navigation.start.position,o->published,12);memcpy(o->navigation.start.query_point,o->published,12);
    o->flight.nav.start_valid=1;
    if(o->navigation.start.radius!=o->navigation_radius||o->navigation.start.height!=o->death_item_extent_7c4||o->navigation.start.word_040)return RF_FORMAT;
    if(!n){*found=0;return RF_OK;}
    c.threat=threat;c.radius=o->navigation_radius;c.height=o->death_item_extent_7c4;
    c.solid.rooms=stream->collision->views;c.solid.room_count=stream->collision->room_count;
    c.solid.primary=stream->collision->primary;c.solid.primary_count=stream->collision->primary_count;
    c.solid.children=stream->collision->children;c.solid.child_count=stream->collision->child_count;
    mode=scene_flight_grounded_mode(campaign_modes[o->movement_slot].index);
    saved_goal=w->references[n];saved_start=w->references[n+1];scene_flight_search_busy=1;
    /* Reserved goal is a finite, unreachable scratch placeholder for the core
     * API's ordinary-goal range validation. It is never inserted or retained. */
    w->references[n]=(rf_entity_navigation_reference){&dummy,n+1,NULL,0};
    w->references[n+1]=(rf_entity_navigation_reference){&o->navigation.start,n+2,links,0};
    status=rf_entity_navigation_search_prepare(w->references,n,c.radius,c.height,mode);if(status)goto done;
    status=rf_entity_navigation_connect_start(&list,pair[0],pair[1],o->navigation.start.query_point,
        threat->handle,scene_flight_nearest,&c,&value);if(status||!value)goto done;
    for(uint32_t i=0;i<list.count;i++){
        if(!tokens[i]||tokens[i]>n+2||tokens[i]==n+1){status=RF_FORMAT;goto done;}
        links[i]=tokens[i]-1;
    }
    w->references[n+1].neighbor_count=list.count;
    q.start=n+1;q.goal=n;q.alternate=threat->handle;q.height=c.height;q.edge_parameter=4.0f;
    status=rf_entity_navigation_search_solid_members(w->references,n+2,0,n,&q,w->scratch,n+2,
        &c.solid,threat->position,&route,&value);if(status)goto done;
    if(!scene_flight_owner_live(o)||!scene_flight_threat_live(threat)){status=RF_NOT_FOUND;goto done;}
    if(value&&route.count>=2){
        for(uint32_t i=0;i<route.count;i++){status=scene_flight_node_capture(o,route.nodes[i],prefix+i);if(status)goto done;}
        memcpy(o->flight.nav.prefix,prefix,sizeof(prefix));o->flight.nav.count=route.count;
        o->flight.nav.previous=0;o->flight.nav.current=1;o->flight.nav.wait_134=now;
        memcpy(o->flight.nav.request_eye_14c,o->eye_position,12);o->flight.nav.eye_valid=1;
        if(o->flight.qualified==2){
            /* Finite armed bridge starts from the REAL resolved node1. The
             * follower consumes this point before its first arrival test. */
            o->flight.nav.pair[0]=prefix[0];o->flight.nav.pair[1]=prefix[1];
            status=scene_flight_node_resolve(o,prefix[1],&node);if(status||!node){if(!status)status=RF_FORMAT;goto done;}
            memcpy(o->flight.nav.steering_174,node->position,12);
            if(mode)o->flight.nav.steering_174[1]=o->eye_position[1];
        }
    }else value=0;
done:
    w->references[n]=saved_goal;w->references[n+1]=saved_start;scene_flight_search_busy=0;
    if(!status)*found=!!value;return status;
}
static int scene_flight_follower(scene_stream *stream,campaign_npc_body *o,
    const scene_medic_flight_threat_view *threat,int32_t now,uint32_t *active,uint32_t *no_route)
{
    scene_medic_flight_nav *n=&o->flight.nav;rf_entity_navigation_candidate *previous,*current;
    float eye[3],direction[3],lateral[3],point[3],threshold=.3f,fraction,offset,minimum;
    double length,distance,a,b;uint32_t grounded,cls,classification,found;int status;
    *active=*no_route=0;
    status=scene_flight_node_resolve(o,n->prefix[n->current],&current);if(status||!current)return status?status:RF_FORMAT;
    grounded=scene_flight_grounded_mode(campaign_modes[o->movement_slot].index);
    memcpy(eye,o->eye_position,12);if(grounded)eye[1]=current->position[1];
    a=(float)scene_flight_distance(o->published,n->steering_174);
    b=(float)scene_flight_distance(current->position,eye);distance=a<b?a:b;
    cls=campaign_seeds.items[o-campaign_npc_bodies].class_index;
    if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    if(campaign_seeds.classes[cls].physics.flags&0x800u)threshold=2;
    if(campaign_seeds.classes[cls].physics.flags&(0x400000u|0x200u))threshold=3;
    if(campaign_seeds.classes[cls].physics.flags2&0x40u)threshold=o->model_radius_78<=2.5f?.5f:1.3f;
    if(!isfinite(distance))return RF_FORMAT;
    if(n->approach_15c){
        classification=0;
        if(!(distance<threshold)){
            status=rf_entity_navigation_single(o->published,o->navigation_radius,o->death_item_extent_7c4,
                grounded,current,&classification);if(status)return status;
        }
        if(!classification){
            n->approach_15c=0;
            if(o->flight.qualified==2){
                /* Approved finite-prefix substitution: advance this admitted
                 * route once. Never issue the medic approach-boundary query. */
                rf_entity_navigation_route cursor={0};
                cursor.word_000=n->count;cursor.word_014=n->previous;cursor.word_018=n->current;
                status=rf_entity_navigation_advance(&cursor);
                if(status==-1){scene_flight_nav_clear(n,now);*no_route=1;return RF_OK;}
                if(status)return status;
                n->previous=cursor.word_014;n->current=cursor.word_018;
                n->pair[0]=n->prefix[n->previous];n->pair[1]=n->prefix[n->current];
            }else{
                n->pair[0]=n->prefix[n->current];n->pair[1]=(scene_medic_flight_node){0,0};
                if(scene_flight_threat_live(threat)){
                    status=scene_medic_flight_nav_request(stream,o,threat,now,&found);if(status)return status;
                    if(n->count<=2){scene_flight_nav_clear(n,now);*no_route=1;return RF_OK;}
                    n->approach_15c=0;n->previous=1;n->current=2;
                }
            }
        }
    }
    if(n->pair[0].kind==SCENE_FLIGHT_NODE_NONE)return RF_OK;
    status=scene_flight_node_resolve(o,n->prefix[n->current],&current);if(status)return status;
    status=scene_flight_node_resolve(o,n->prefix[n->previous],&previous);if(status)return status;
    if(!current||!previous)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)direction[i]=(float)((double)current->position[i]-previous->position[i]);
    /*40caf0 cross product and4fab70 safe normalization, including zero case. */
    lateral[0]=-direction[2];lateral[1]=0;lateral[2]=direction[0];length=scene_flight_length(lateral);
    if(!isfinite(length))return RF_FORMAT;
    if(length<=0){lateral[0]=1;lateral[1]=lateral[2]=0;}
    else for(uint32_t i=0;i<3;i++)lateral[i]=(float)((double)lateral[i]/length);
    minimum=previous->radius<current->radius?previous->radius:current->radius;
    a=(double)minimum-o->navigation_radius;if(a<0)a=0;offset=(float)(a*n->bias_158);
    for(uint32_t i=0;i<3;i++)lateral[i]=(float)((double)lateral[i]*offset);
    memcpy(point,current->position,12);if(grounded)point[1]=o->eye_position[1];
    for(uint32_t i=0;i<3;i++)point[i]=(float)((double)point[i]+lateral[i]);
    if(!n->approach_15c){
        length=scene_flight_distance(current->position,previous->position);if(!isfinite(length))return RF_FORMAT;
        if(length>0){
            a=(double)previous->radius-o->navigation_radius;if(a<0)a=0;
            fraction=(float)(a/length);if(fraction<0)fraction=0;else if(fraction>1)fraction=1;
            for(uint32_t i=0;i<3;i++){
                float scaled=(float)((double)direction[i]*fraction);
                point[i]=(float)((double)previous->position[i]+scaled);
                point[i]=(float)((double)point[i]+lateral[i]);
            }
            if(grounded)point[1]=o->eye_position[1];
            a=(float)scene_flight_distance(o->published,point);b=(float)scene_flight_distance(o->eye_position,point);
            if((a<b?a:b)<threshold){
                for(uint32_t i=0;i<3;i++)point[i]=(float)((double)current->position[i]+lateral[i]);
                if(grounded)point[1]=o->eye_position[1];
                n->pair[0]=n->prefix[n->previous];n->pair[1]=n->prefix[n->current];n->approach_15c=1;
            }
        }
    }
    if(!scene_flight_finite3(point))return RF_FORMAT;
    memcpy(n->steering_174,point,12);*active=1;return RF_OK;
}
static int scene_flight_actor_blocked(const campaign_npc_body *o,uint32_t *blocked)
{
    *blocked=0;
    /* The separate player registration precedes retained NPC registration.
     * Its actual motion pair comes from the current actor_follow_view call. */
    if(campaign_player_object.view==&campaign_player_view&&
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view&&
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object){
        uint32_t eligible=0;
        if(campaign_player_damage.class_flags&0x20000u){
            if(!scene_flight_player_motion.valid||scene_flight_player_motion.handle!=campaign_player_object.handle||
               scene_flight_player_motion.view!=&campaign_player_view)return RF_NOT_FOUND;
            eligible=scene_flight_player_motion.current==13||scene_flight_player_motion.next==13;
        }else eligible=campaign_player_view.class_type==1&&!(campaign_player_view.flags_7c&0x4000u);
        if(eligible){
            float radius=(float)((double)o->model_radius_78+campaign_player_geometry.model_radius);
            double distance=scene_flight_distance_squared(o->published,rf_scene_actor_pose.public_position);
            if(!isfinite(radius)||!isfinite(distance))return RF_FORMAT;
            if(distance<(double)radius*radius){*blocked=1;return RF_OK;}
        }
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *other=campaign_npc_bodies+i;uint32_t cls;rf_entity_pose *pose;int status;
        if(!scene_flight_owner_live(other))continue;
        if(i>=campaign_seeds.records.count)return RF_FORMAT;cls=campaign_seeds.items[i].class_index;
        if(cls>=campaign_seeds.class_count)return RF_FORMAT;
        if(campaign_seeds.classes[cls].physics.flags&0x20000u){
            if(other==o)continue;
            status=campaign_actor_pose(i,&pose);if(status)return status;
            if(!pose||!rf_motion_has_state(&pose->controller,13))continue;
        }else if(other->view.class_type!=1||(other->object_flags&0x4000u))continue;
        /*506fd0: binary32 summed radii; strict sphere overlap, no health gate. */
        float radius=(float)((double)o->model_radius_78+other->model_radius_78);
        double distance=scene_flight_distance_squared(o->published,other->published);
        if(!isfinite(radius)||!isfinite(distance))return RF_FORMAT;
        if(distance<(double)radius*radius){*blocked=1;return RF_OK;}
    }
    return RF_OK;
}
static int scene_medic_flight_nav_step(scene_stream *stream,campaign_npc_body *o,
    const scene_medic_flight_threat_view *threat,int32_t now,float dt,scene_medic_flight_nav_result *out)
{
    scene_medic_flight_nav_result value={0};scene_medic_flight_nav *n;
    float direction[3],scalar;double length,dot;uint32_t active,blocked;int expired,status;
    if(!stream||!o||!threat||!out||now<0||now>RF_TIMER_PERIOD||!isfinite(dt)||dt<=0)return RF_RANGE;
    n=&o->flight.nav;
    if(!scene_flight_owner_live(o)||o->damage.effects.health<=0||n->current<0||(uint32_t)n->current>=n->count){
        value.source_result=-1;*out=value;return RF_OK;
    }
    if(n->count>4||n->previous<0||(uint32_t)n->previous>=n->count||o->movement_slot>=16)return RF_FORMAT;
    status=rf_timer_expired(n->wait_134,now,&expired);if(status)return status;
    if(!expired){
        if(scene_flight_length(n->turn_138)>0){memcpy(value.turn_point,n->turn_138,12);value.turn_valid=1;}
        *out=value;return RF_OK;
    }
    memset(n->turn_138,0,sizeof(n->turn_138));
    status=scene_flight_follower(stream,o,threat,now,&active,&value.no_route);if(status)return status;
    n->activity_170=active;
    if(!active){*out=value;return RF_OK;}
    memcpy(value.turn_point,(o->ai_mode.flags_530&8u)?threat->position:n->steering_174,12);value.turn_valid=1;
    /* Guard profile retains the ordinary physical sweep below. The original
     * armed anticipatory scan and reciprocal living-actor blocking are deferred;
     * the medic-only cowering actor predicate is not an armed predicate. */
    if(o->flight.qualified!=2){
        status=scene_flight_actor_blocked(o,&blocked);if(status)return status;
        if(blocked){value.source_result=-2;*out=value;return RF_OK;}
    }
    for(uint32_t i=0;i<3;i++)direction[i]=(float)((double)n->steering_174[i]-o->eye_position[i]);
    length=scene_flight_length(direction);
    /*4faaf0 has no zero guard. Do not synthesize nonfinite command storage. */
    if(!(length>0)||!isfinite(length))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)direction[i]=(float)((double)direction[i]/length);
    dot=(double)o->look.orientation[8]*direction[2];dot+=(double)o->look.orientation[7]*direction[1];
    dot+=(double)o->look.orientation[6]*direction[0];if(!isfinite(dot))return RF_FORMAT;
    scalar=(o->ai_mode.flags_530&8u)?1:dot<0?0:(float)dot;if(scalar>1)scalar=1;
    for(uint32_t i=0;i<3;i++)value.command[i]=(float)((double)direction[i]*scalar);
    value.command_valid=1;*out=value;return RF_OK;
}
#endif
