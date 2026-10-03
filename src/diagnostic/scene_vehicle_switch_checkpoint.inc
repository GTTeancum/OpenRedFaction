#ifndef RF_SCENE_VEHICLE_SWITCH_CHECKPOINT_INCLUDED
#define RF_SCENE_VEHICLE_SWITCH_CHECKPOINT_INCLUDED
/* RFSW1/2/3/4 is the outermost vehicle envelope. Version2 uses reserved row+60
 * for a valid parked owner's ground profile; all other offsets are unchanged.
 * Version3 admits profile4; its row+16/+160 hold torpedo reserve/cooldown,
 * with unused ground weapon fields zero. Ground rows retain version2 meaning.
 * Version4 admits Fighter profile5: row+176 is rocket cooldown and the unused
 * aim-angle words +180/+184/+188 are rocket warmup/held/shot count.
 * Explicit little-endian fields;
 * authored UIDs and passive order survive, runtime handles/pointers do not.
 * Include after switch and physics checkpoint implementation. */
struct scene_vehicle_switch_checkpoint_stage {
 uint32_t count,bytes,active,profile,admitted;scene_vehicle_parked_state *rows;
};
/* valid,activeUID,profile,count,restored banks,first bank UID/ammo/RNG,
 * second bank UID/ammo/RNG,active ammo/RNG,active handle,first bank handle,status */
uint32_t rf_scene_vehicle_switch_restore[16];
static uint32_t scene_vehicle_switch_checkpoint_bytes(const scene_vehicle_switch_checkpoint_stage *s){return s?s->bytes:0;}
static void scene_vehicle_switch_checkpoint_close(scene_vehicle_switch_checkpoint_stage **s)
{if(s&&*s){free((*s)->rows);free(*s);*s=NULL;}}
static int scene_vehicle_switch_checkpoint_peek(const unsigned char *p,uint32_t n,uint32_t *uid,uint32_t *profile,uint32_t *count)
{
 uint64_t size;uint32_t c,inner,a,k,version;
 if(!uid||!profile||!count||(!p&&n))return RF_RANGE;*uid=*profile=*count=0;
 if(n<4||memcmp(p,"RFSW",4))return RF_OK;
 if(n<SCENE_VEHICLE_SWITCH_HEADER)return RF_FORMAT;
 version=scene_history_word(p+4);if(version<1||version>4)return RF_FORMAT;
 inner=scene_history_word(p+8);a=scene_history_word(p+12);k=scene_history_word(p+16);c=scene_history_word(p+20);
 size=SCENE_VEHICLE_SWITCH_HEADER+(uint64_t)inner+(uint64_t)c*SCENE_VEHICLE_SWITCH_ROW;
 if(!a||a==UINT32_MAX||k<1||k>(version>=3?version+1:3u)||!c||c>RF_OBJECT_CAPACITY||size!=n||size>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
 for(uint32_t i=0;i<c;i++){
  const unsigned char *r=p+SCENE_VEHICLE_SWITCH_HEADER+inner+i*SCENE_VEHICLE_SWITCH_ROW;uint32_t u=scene_history_word(r);
  if(!u||u==UINT32_MAX||u==a||scene_history_word(r+4)>1)return RF_FORMAT;
  if(scene_history_word(r+316))return RF_FORMAT;
  if(!scene_history_word(r+4)){for(uint32_t j=8;j<SCENE_VEHICLE_SWITCH_ROW;j++)if(r[j])return RF_FORMAT;}
  else {uint32_t profile=scene_history_word(r+60);
   if((version==1&&profile)||(version>=2&&(profile<1||profile>(version>=3?version+1:3u))))return RF_FORMAT;}
  for(uint32_t j=0;j<i;j++)if(scene_history_word(p+SCENE_VEHICLE_SWITCH_HEADER+inner+j*SCENE_VEHICLE_SWITCH_ROW)==u)return RF_FORMAT;
 }
 *uid=a;*profile=k;*count=c;return RF_OK;
}
static int scene_vehicle_switch_checkpoint_row_identity(const unsigned char *p,uint32_t n,uint32_t index,
 uint32_t *uid,uint32_t *valid,uint32_t *profile)
{
 uint32_t a,k,c;int st;if(!uid||!valid||!profile)return RF_RANGE;
 st=scene_vehicle_switch_checkpoint_peek(p,n,&a,&k,&c);if(st)return st;if(!a||index>=c)return RF_RANGE;
 const unsigned char *row=p+SCENE_VEHICLE_SWITCH_HEADER+scene_history_word(p+8)+index*SCENE_VEHICLE_SWITCH_ROW;
 *uid=scene_history_word(row);*valid=scene_history_word(row+4);
 *profile=*valid?(scene_history_word(p+4)==1?k:scene_history_word(row+60)):0;return RF_OK;
}
static int scene_vehicle_switch_checkpoint_basis(const float *m)
{
 for(uint32_t i=0;i<9;i++)if(!isfinite(m[i]))return RF_FORMAT;
 for(uint32_t i=0;i<3;i++)for(uint32_t j=i;j<3;j++){
  double d=0;for(uint32_t k=0;k<3;k++)d+=(double)m[3*i+k]*m[3*j+k];
  if(fabs(d-(i==j?1.:0.))>.015)return RF_FORMAT;
 }
 double determinant=m[0]*(m[4]*m[8]-m[5]*m[7])-m[1]*(m[3]*m[8]-m[5]*m[6])+m[2]*(m[3]*m[7]-m[4]*m[6]);
 return fabs(determinant-1.)<=.025?RF_OK:RF_FORMAT;
}
/* Float fields are written individually, never by serializing a C struct. */
static void scene_vehicle_switch_checkpoint_floats(unsigned char *p,const float *v,uint32_t n)
{for(uint32_t i=0;i<n;i++)scene_history_put_float(p+i*4,v[i]);}
static int scene_vehicle_switch_checkpoint_readfloats(const unsigned char *p,float *v,uint32_t n)
{for(uint32_t i=0;i<n;i++){v[i]=scene_history_float(p+i*4);if(!isfinite(v[i])||fabsf(v[i])>1e9f)return RF_FORMAT;}return RF_OK;}
static int scene_vehicle_switch_checkpoint_valid(const scene_stream *s,const scene_vehicle_parked_state *v)
{
 uint32_t quiet,primary_capacity,secondary_capacity,submarine_capacity=0;float submarine_period=0;
 int32_t seat;int st;const scene_vehicle_profile_pack *pack;
 if(!v->valid)return RF_OK;
 if(v->profile<1||v->profile>5)return RF_FORMAT;
 pack=scene_vehicle_profile_pack_get(s,v->profile);
 if(pack){primary_capacity=v->profile==4?0:pack->primary.capacity;secondary_capacity=v->profile==2?pack->secondary.capacity:0;
  if(v->profile==4){submarine_capacity=pack->submarine_definition.capacity;submarine_period=pack->submarine_definition.fire_seconds;}
  if(v->profile==5){primary_capacity=pack->fighter_primary.capacity;secondary_capacity=pack->fighter_rocket.capacity;}
  seat=v->jeep_role?pack->gunner_seat:pack->seat;}
 else {
  /* Ordinary same-class scenes need no optional preload/adoption allocation. */
  if(!s||!s->driller||!s->driller_runtime||v->profile!=rf_scene_vehicle_enabled)return RF_NOT_FOUND;
  primary_capacity=v->profile==4?0:s->apc_primary.definition.capacity;secondary_capacity=v->profile==2?s->apc_secondary.definition.capacity:0;
  if(v->profile==4){submarine_capacity=s->submarine_weapon.definition.capacity;submarine_period=s->submarine_weapon.definition.fire_seconds;}
  if(v->profile==5){primary_capacity=s->fighter_weapon.primary.capacity;secondary_capacity=s->fighter_weapon.rocket.capacity;}
  if(v->profile==3){st=scene_jeep_seat_index(s->driller,v->jeep_role,&seat);if(st)return st;}
  else seat=s->driller->seat;
 }
 if(v->profile==4){
  if(!isfinite(submarine_period)||submarine_period<=0||!isfinite(v->submarine_cooldown)||
     v->submarine_cooldown<0||v->submarine_cooldown>submarine_period+.0001f||
     v->submarine_reserve<0||(uint32_t)v->submarine_reserve>submarine_capacity)return RF_FORMAT;
 }else if(v->submarine_reserve||v->submarine_cooldown!=0)return RF_FORMAT;
 if(v->profile==5){
  if(v->random.value||v->aim_active||v->fighter_secondary.held||
     !isfinite(v->fighter_secondary.warmup)||v->fighter_secondary.warmup>0||
     !isfinite(v->fighter_secondary.cooldown)||v->fighter_secondary.cooldown>0||
     v->fighter_secondary.cooldown!=v->secondary_cooldown)return RF_NOT_FOUND;
  for(uint32_t i=0;i<3;i++)if(v->aim.angles[i]!=0||v->aim_eye[i]!=0)return RF_FORMAT;
  for(uint32_t i=0;i<9;i++)if(v->aim_basis[i]!=0)return RF_FORMAT;
 }else if(v->fighter_secondary.cooldown!=0||v->fighter_secondary.warmup!=0||
          v->fighter_secondary.held||v->fighter_secondary.shots)return RF_FORMAT;
 if(v->scripted_physics>1||v->physics.skip_forces>1||v->aim_active>1||v->jeep_role>1||
    v->primary_reserve<0||v->secondary_reserve<0||
    (uint32_t)v->primary_reserve>primary_capacity||
    (uint32_t)v->secondary_reserve>secondary_capacity||
    v->primary.held||v->primary.warmup>0||v->primary.cooldown>0||v->secondary_cooldown>0||
    v->drill.held||v->drill.contact_active||v->view.weapon_owner||v->view.linked_handle!=-1||
    v->route.active||v->route.path.count||v->route.event||v->route.pursuit)return RF_NOT_FOUND;
 if(v->scripted_physics&&!(v->body_flags&0x80000000u)&&(v->body_flags&0x18000000u)!=0x18000000u)return RF_FORMAT;
 if(v->seat!=seat||(v->profile!=3&&v->jeep_role))return RF_FORMAT;
 st=scene_vehicle_switch_checkpoint_basis(v->physics.orientation);if(st)return st;
 if(v->aim_active){st=scene_vehicle_switch_checkpoint_basis(v->aim_basis);if(st)return st;}
 st=scene_vehicle_switch_quiet(&v->physics,&quiet);if(st)return st;return quiet?RF_OK:RF_NOT_FOUND;
}
static void scene_vehicle_switch_checkpoint_encode(unsigned char *p,const scene_vehicle_parked_state *v,uint32_t version)
{
 memset(p,0,SCENE_VEHICLE_SWITCH_ROW);scene_history_put(p,v->uid);scene_history_put(p+4,v->valid);if(!v->valid)return;
 scene_history_put(p+8,v->body_flags);scene_history_put(p+12,v->scripted_physics);
 scene_history_put(p+16,(uint32_t)(v->profile==4?v->submarine_reserve:v->primary_reserve));scene_history_put(p+20,(uint32_t)v->secondary_reserve);
 scene_history_put(p+24,v->random.value);scene_history_put(p+28,(uint32_t)v->seat);
 scene_history_put(p+32,v->jeep_role);scene_history_put(p+36,v->aim_active);scene_history_put(p+40,(uint32_t)v->route.ai_action);
 scene_history_put(p+44,v->view.flags_7d0);scene_history_put(p+48,v->view.flags_810);scene_history_put(p+52,(uint32_t)v->view.action_520);
 scene_history_put(p+56,v->physics.skip_forces);
 if(version>=2)scene_history_put(p+60,v->profile);
 scene_vehicle_switch_checkpoint_floats(p+64,v->physics.position,3);scene_vehicle_switch_checkpoint_floats(p+76,v->physics.orientation,9);
 scene_vehicle_switch_checkpoint_floats(p+112,v->physics.velocity,3);scene_vehicle_switch_checkpoint_floats(p+124,v->physics.momentum,3);
 scene_vehicle_switch_checkpoint_floats(p+136,v->physics.force,3);scene_vehicle_switch_checkpoint_floats(p+148,v->physics.torque,3);
 scene_history_put_float(p+160,v->profile==4?v->submarine_cooldown:v->primary.cooldown);scene_history_put_float(p+164,v->primary.warmup);
 scene_history_put(p+168,v->primary.held);scene_history_put(p+172,v->primary.shots);scene_history_put_float(p+176,v->secondary_cooldown);
 if(v->profile==5){scene_history_put_float(p+180,v->fighter_secondary.warmup);
  scene_history_put(p+184,v->fighter_secondary.held);scene_history_put(p+188,v->fighter_secondary.shots);}
 else scene_vehicle_switch_checkpoint_floats(p+180,v->aim.angles,3);
 scene_vehicle_switch_checkpoint_floats(p+192,v->aim_eye,3);
 scene_vehicle_switch_checkpoint_floats(p+204,v->aim_basis,9);
 scene_history_put_float(p+240,v->drill.held_seconds);scene_history_put_float(p+244,v->drill.damage_wait);
 scene_history_put_float(p+248,v->drill.contact_seconds);scene_history_put_float(p+252,v->drill.spin);scene_history_put_float(p+256,v->drill.phase);
 scene_history_put(p+260,v->drill.held);scene_history_put(p+264,v->drill.contact_active);scene_history_put(p+268,v->drill.accepted_cuts);
 scene_history_put(p+272,v->drill.contacts);scene_history_put(p+276,v->drill.damage_calls);
 scene_history_put(p+280,v->route.index);scene_history_put(p+284,v->route.mode);scene_history_put(p+288,v->route.reverse);
 scene_vehicle_switch_checkpoint_floats(p+292,v->route.target,3);
 scene_history_put(p+304,(uint32_t)v->view.weapons[0]);scene_history_put(p+308,(uint32_t)v->view.weapons[1]);scene_history_put_float(p+312,v->view.base_speed);
}
static int scene_vehicle_switch_checkpoint_decode(const scene_stream *s,const unsigned char *p,uint32_t version,
 uint32_t active_profile,scene_vehicle_parked_state *v)
{
 int st;memset(v,0,sizeof(*v));v->uid=scene_history_word(p);v->valid=scene_history_word(p+4);
 if(!v->valid){for(uint32_t i=8;i<SCENE_VEHICLE_SWITCH_ROW;i++)if(p[i])return RF_FORMAT;return RF_OK;}
 if(v->valid!=1||version<1||version>4||(version==1&&scene_history_word(p+60))||scene_history_word(p+316))return RF_FORMAT;
 v->profile=version==1?active_profile:scene_history_word(p+60);if(v->profile<1||v->profile>(version>=3?version+1:3u))return RF_FORMAT;
 const scene_vehicle_profile_pack *pack=scene_vehicle_profile_pack_get(s,v->profile);
 if(pack){v->view=pack->view;v->drill=pack->drill;memcpy(v->physics.inverse_inertia,pack->physics.state.inverse_inertia,36);}
 else {
  if(!s||!s->driller_runtime||v->profile!=rf_scene_vehicle_enabled)return RF_NOT_FOUND;
  v->view=s->driller_runtime->entry.view;v->drill=s->driller_weapon;
  memcpy(v->physics.inverse_inertia,s->driller_runtime->physics.state.inverse_inertia,36);
 }
 v->view.linked_handle=-1;v->view.weapon_owner=NULL;v->view.occupants=NULL;
 v->body_flags=scene_history_word(p+8);v->scripted_physics=scene_history_word(p+12);
 v->primary_reserve=(int32_t)scene_history_word(p+16);v->secondary_reserve=(int32_t)scene_history_word(p+20);
 v->random.value=scene_history_word(p+24);v->seat=(int32_t)scene_history_word(p+28);v->jeep_role=scene_history_word(p+32);v->aim_active=scene_history_word(p+36);
 v->route.ai_action=(int32_t)scene_history_word(p+40);v->view.flags_7d0=scene_history_word(p+44);v->view.flags_810=scene_history_word(p+48);v->view.action_520=(int32_t)scene_history_word(p+52);
 v->physics.skip_forces=scene_history_word(p+56);
 #define RFSW_FLOATS(offset,field,n) do{st=scene_vehicle_switch_checkpoint_readfloats(p+(offset),(field),(n));if(st)return st;}while(0)
 RFSW_FLOATS(64,v->physics.position,3);RFSW_FLOATS(76,v->physics.orientation,9);RFSW_FLOATS(112,v->physics.velocity,3);
 RFSW_FLOATS(124,v->physics.momentum,3);RFSW_FLOATS(136,v->physics.force,3);RFSW_FLOATS(148,v->physics.torque,3);
 RFSW_FLOATS(160,&v->primary.cooldown,1);RFSW_FLOATS(164,&v->primary.warmup,1);v->primary.held=scene_history_word(p+168);v->primary.shots=scene_history_word(p+172);
 RFSW_FLOATS(176,&v->secondary_cooldown,1);
 if(v->profile==5){v->fighter_secondary.cooldown=v->secondary_cooldown;
  RFSW_FLOATS(180,&v->fighter_secondary.warmup,1);v->fighter_secondary.held=scene_history_word(p+184);v->fighter_secondary.shots=scene_history_word(p+188);}
 else {RFSW_FLOATS(180,v->aim.angles,3);}
 RFSW_FLOATS(192,v->aim_eye,3);RFSW_FLOATS(204,v->aim_basis,9);
 RFSW_FLOATS(240,&v->drill.held_seconds,1);RFSW_FLOATS(244,&v->drill.damage_wait,1);RFSW_FLOATS(248,&v->drill.contact_seconds,1);
 RFSW_FLOATS(252,&v->drill.spin,1);RFSW_FLOATS(256,&v->drill.phase,1);
 v->drill.held=scene_history_word(p+260);v->drill.contact_active=scene_history_word(p+264);v->drill.accepted_cuts=scene_history_word(p+268);
 v->drill.contacts=scene_history_word(p+272);v->drill.damage_calls=scene_history_word(p+276);
 v->route.index=scene_history_word(p+280);v->route.mode=scene_history_word(p+284);v->route.reverse=scene_history_word(p+288);
 RFSW_FLOATS(292,v->route.target,3);v->view.weapons[0]=(int32_t)scene_history_word(p+304);v->view.weapons[1]=(int32_t)scene_history_word(p+308);RFSW_FLOATS(312,&v->view.base_speed,1);
 #undef RFSW_FLOATS
 if(v->profile==4){v->submarine_reserve=v->primary_reserve;v->primary_reserve=0;
  v->submarine_cooldown=v->primary.cooldown;v->primary.cooldown=0;}
 return scene_vehicle_switch_checkpoint_valid(s,v);
}
static int scene_vehicle_switch_checkpoint_live(const scene_stream *s,uint32_t i,scene_vehicle_parked_state *v)
{
 const scene_passive_vehicle *p=campaign_passive_vehicles+i;*v=scene_vehicle_parked[i];
 if(!v->valid){memset(v,0,sizeof(*v));v->uid=p->uid;return RF_OK;}
 if(v->profile<1||v->profile>5||v->uid!=p->uid||v->handle!=p->handle||p->attached||p->group_owned||p->resource_kind!=(v->profile==5?1:v->profile==4?0:v->profile+2)||
    rf_object_registry_lookup(&campaign_registry,p->handle)!=p)return RF_NOT_FOUND;
 memcpy(v->physics.position,p->pose.position,12);memcpy(v->physics.orientation,p->pose.input_matrix,36);
 v->scripted_physics=p->scripted_physics;if(p->scripted_physics)v->body_flags=p->physics_body_flags;
 return scene_vehicle_switch_checkpoint_valid(s,v);
}
static int scene_vehicle_switch_checkpoint_capture(const scene_stream *s,unsigned char *p,uint32_t cap,uint32_t inner,uint32_t *written)
{
 uint64_t size;uint32_t version=1;int st;scene_vehicle_parked_state v;unsigned char scratch[SCENE_VEHICLE_SWITCH_ROW];
 if(!s||!p||!written||inner>cap)return RF_RANGE;if(!scene_vehicle_switch_has_parked()){*written=inner;return RF_OK;}
 if(!s->driller_runtime||rf_scene_vehicle_enabled<1||rf_scene_vehicle_enabled>5||scene_vehicle_switch_stream!=s||
    !scene_vehicle_parked||scene_vehicle_parked_count!=campaign_passive_vehicle_count)return RF_NOT_FOUND;
 size=SCENE_VEHICLE_SWITCH_HEADER+(uint64_t)inner+(uint64_t)scene_vehicle_parked_count*SCENE_VEHICLE_SWITCH_ROW;
 if(size>cap||size>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
 if(scene_vehicle_switch_boot_original_profile()!=rf_scene_vehicle_enabled)version=2;
 for(uint32_t i=0;i<scene_vehicle_parked_count;i++)
  if(scene_vehicle_parked[i].valid&&scene_vehicle_parked[i].profile!=rf_scene_vehicle_enabled)version=2;
 if(rf_scene_vehicle_enabled==4)version=3;
 for(uint32_t i=0;i<scene_vehicle_parked_count;i++)
  if(scene_vehicle_parked[i].valid&&scene_vehicle_parked[i].profile==4)version=3;
 if(rf_scene_vehicle_enabled==5)version=4;
 for(uint32_t i=0;i<scene_vehicle_parked_count;i++)
  if(scene_vehicle_parked[i].valid&&scene_vehicle_parked[i].profile==5)version=4;
 for(uint32_t i=0;i<scene_vehicle_parked_count;i++){
  st=scene_vehicle_switch_checkpoint_live(s,i,&v);if(st)return st;
  scene_vehicle_switch_checkpoint_encode(scratch,&v,version);st=scene_vehicle_switch_checkpoint_decode(s,scratch,version,rf_scene_vehicle_enabled,&v);if(st)return st;
 }
 memmove(p+SCENE_VEHICLE_SWITCH_HEADER,p,inner);memcpy(p,"RFSW",4);scene_history_put(p+4,version);scene_history_put(p+8,inner);
 scene_history_put(p+12,(uint32_t)campaign_authored_vehicle_uid);scene_history_put(p+16,rf_scene_vehicle_enabled);scene_history_put(p+20,scene_vehicle_parked_count);
 for(uint32_t i=0;i<scene_vehicle_parked_count;i++){scene_vehicle_switch_checkpoint_live(s,i,&v);scene_vehicle_switch_checkpoint_encode(p+SCENE_VEHICLE_SWITCH_HEADER+inner+i*SCENE_VEHICLE_SWITCH_ROW,&v,version);}
 *written=(uint32_t)size;return RF_OK;
}
static int scene_vehicle_switch_checkpoint_prepare(const scene_stream *s,const unsigned char *p,uint32_t n,uint32_t budget,
 scene_vehicle_switch_checkpoint_stage **out,const unsigned char **inner,uint32_t *innerbytes)
{
 uint32_t a,k,c;uint64_t cost;int st;scene_vehicle_switch_checkpoint_stage *v;
 if(!s||!out||*out||!inner||!innerbytes)return RF_RANGE;*inner=p;*innerbytes=n;
 st=scene_vehicle_switch_checkpoint_peek(p,n,&a,&k,&c);if(st||!a)return st;
 if(!s->driller_runtime||a!=(uint32_t)campaign_authored_vehicle_uid||k!=rf_scene_vehicle_enabled||c!=campaign_passive_vehicle_count)return RF_NOT_FOUND;
 cost=sizeof(*v)+(uint64_t)c*sizeof(*v->rows);if(cost>budget||(uint64_t)c*sizeof(*v->rows)>256u*1024u)return RF_RANGE;
 v=calloc(1,sizeof(*v));if(!v)return RF_IO;v->rows=calloc(c,sizeof(*v->rows));if(!v->rows){free(v);return RF_IO;}
 v->active=a;v->profile=k;v->count=c;v->bytes=(uint32_t)cost;
 for(uint32_t i=0;i<c;i++){
  const scene_passive_vehicle *owner=campaign_passive_vehicles+i;
  st=scene_vehicle_switch_checkpoint_decode(s,p+SCENE_VEHICLE_SWITCH_HEADER+scene_history_word(p+8)+i*SCENE_VEHICLE_SWITCH_ROW,scene_history_word(p+4),k,v->rows+i);
  if(!st&&v->rows[i].uid!=owner->uid)st=RF_FORMAT;
  if(!st&&v->rows[i].valid&&(owner->attached||owner->group_owned||owner->resource_kind!=(v->rows[i].profile==5?1:v->rows[i].profile==4?0:v->rows[i].profile+2)))st=RF_NOT_FOUND;
  if(st){scene_vehicle_switch_checkpoint_close(&v);return st;}v->rows[i].handle=owner->handle;v->rows[i].view.handle=(int32_t)owner->handle;
 }
 *out=v;*inner=p+SCENE_VEHICLE_SWITCH_HEADER;*innerbytes=scene_history_word(p+8);return RF_OK;
}
static int scene_vehicle_switch_checkpoint_admit(const scene_stream *s,scene_vehicle_switch_checkpoint_stage *stage,
 const scene_vehicle_checkpoint_record *vehicle,const scene_passive_vehicle_checkpoint_stage *passive,const scene_vehicle_physics_checkpoint_stage *physics)
{
 if(!stage)return RF_OK;
 if(!s||!vehicle||vehicle->profile!=stage->profile||!passive||passive->version!=2||passive->count!=stage->count)return RF_FORMAT;
 for(uint32_t i=0;i<stage->count;i++){
  scene_vehicle_parked_state *v=stage->rows+i;const scene_passive_vehicle_saved *p=passive->rows+i;
  if(!p->loaded||p->uid!=v->uid)return RF_FORMAT;if(!v->valid)continue;
  if(p->attached||memcmp(p->position,v->physics.position,12)||memcmp(p->basis,v->physics.orientation,36))return RF_FORMAT;
  if(physics){const scene_vehicle_physics_saved *f;if(physics->count!=stage->count+1)return RF_FORMAT;f=physics->rows+i+1;
   if(f->uid!=v->uid||f->role!=1||f->scripted!=v->scripted_physics||(f->scripted&&f->body_flags!=v->body_flags))return RF_FORMAT;
  }else if(v->scripted_physics)return RF_FORMAT;
  v->view.flags_7c=p->object_flags;v->view.flags_814=p->flags_814;
 }
 stage->admitted=1;return RF_OK;
}
/* Infallible transfer after all host, RFVA and RFPV publication. */
static void scene_vehicle_switch_checkpoint_assign(scene_stream *s,scene_vehicle_switch_checkpoint_stage *stage)
{
 uint32_t *p=rf_scene_vehicle_switch_restore;memset(p,0,sizeof(rf_scene_vehicle_switch_restore));
 if(!stage||!stage->admitted)return;
 scene_vehicle_switch_close();scene_vehicle_parked=stage->rows;stage->rows=NULL;
 scene_vehicle_parked_count=stage->count;scene_vehicle_switch_stream=s;scene_vehicle_switch_count=1;
 p[0]=1;p[1]=stage->active;p[2]=stage->profile;p[3]=stage->count;
 for(uint32_t i=0;i<stage->count;i++)if(scene_vehicle_parked[i].valid){
  const scene_vehicle_parked_state *v=scene_vehicle_parked+i;uint32_t index=p[4]++;
  if(index<2){p[5+3*index]=v->uid;p[6+3*index]=(uint32_t)(v->profile==4?v->submarine_reserve:v->primary_reserve);p[7+3*index]=v->profile==4?0:v->random.value;}
  if(!index)p[14]=v->handle;
 }
 p[11]=(uint32_t)(rf_scene_vehicle_enabled==5?s->fighter_weapon.primary_reserve:rf_scene_vehicle_enabled==4?s->submarine_weapon.reserve:s->apc_primary.reserve);
 p[12]=rf_scene_vehicle_enabled>=4?0:s->apc_primary.random.value;p[13]=s->driller_runtime->entry.host.handle;
}
#endif

