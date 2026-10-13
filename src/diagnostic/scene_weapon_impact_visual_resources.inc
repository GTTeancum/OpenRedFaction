#ifndef RF_SCENE_WEAPON_IMPACT_VISUAL_RESOURCES_INC
#define RF_SCENE_WEAPON_IMPACT_VISUAL_RESOURCES_INC
/* Optional, deduplicated conventional dry-world impact assets. Include before
 * the particle bitmap resolver. No archive or input-table pointer survives. */
#define SCENE_WEAPON_IMPACT_VISUAL_BUDGET (512u*1024u)
#define SCENE_WEAPON_IMPACT_VISUAL_SCRATCH (16u*1024u)
#define SCENE_WEAPON_IMPACT_VISUAL_HEADROOM (256u*1024u)
#define SCENE_WEAPON_IMPACT_VISUAL_TAG 0x60000000u
#define SCENE_WEAPON_IMPACT_VISUAL_INSTANCES 32u
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
typedef struct scene_weapon_impact_visual_profile {
    uint32_t count,kinds[2];float radii[2];
} scene_weapon_impact_visual_profile;
typedef struct scene_weapon_impact_visual_instance {
    rf_explosion_clock clock;uint32_t kind,slot;
} scene_weapon_impact_visual_instance;
typedef struct scene_weapon_impact_visual_owner {
    rf_explosion_definition definitions[3];rf_vclip_definition smoke;
    rf_explosion_materials materials;
    scene_weapon_impact_visual_profile profiles[64];
    scene_weapon_impact_visual_instance instances[SCENE_WEAPON_IMPACT_VISUAL_INSTANCES];
    rf_random_state random;uint32_t resident_bytes,peak_bytes;
} scene_weapon_impact_visual_owner;
static scene_weapon_impact_visual_owner *scene_weapon_impact_visual;
/* ready,contacts,central births,sparks,smoke,pool drops,wet/room omissions,
 * errors,last status,resident bytes,peak bytes,free pages,active emitters,
 * successful-load resets,world queries,world matches */
uint32_t rf_scene_weapon_impact_visual[16];
static int scene_weapon_impact_visual_equal(const char *a,const char *b)
{
    uint32_t x,y;do{x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;
    }while(x);return 1;
}
static const char *const scene_weapon_impact_visual_names[]={
    "12mm handgun","Undercover 12mm handgun","Assault Rifle","Shotgun",
    "Machine Pistol","Machine Pistol Special","Sniper Rifle","heavy_machine_gun",
    "scope_assault_rifle","Vauss","Tankbot Chaingun"};
static char *scene_weapon_impact_visual_space(char *p)
{while(*p==' '||*p=='\t')++p;return p;}
static int scene_weapon_impact_visual_string(char **input,char value[64])
{
    char *p=scene_weapon_impact_visual_space(*input);uint32_t n=0;
    if(*p++!='"')return RF_FORMAT;
    while(*p && *p!='"'){if(n==63)return RF_RANGE;value[n++]=*p++;}
    if(*p++!='"' || !n)return RF_FORMAT;value[n]=0;*input=p;return RF_OK;
}
/* NXDK strtof is an assertion-only stub. Parse the authored positive decimal
 * token directly, as the other shared table readers do. Only the supported
 * .25/.3 radii are admitted; a failed token never advances the caller. */
static int scene_weapon_impact_visual_radius(char **input,float *out)
{
    char *p=scene_weapon_impact_visual_space(*input);double value=0,scale=.1;uint32_t digits=0;
    if(*p=='+')++p;
    while(*p>='0'&&*p<='9'){value=value*10+(*p++-'0');++digits;if(value>1)return RF_FORMAT;}
    if(*p=='.'){++p;while(*p>='0'&&*p<='9'){value+=(*p++-'0')*scale;scale*=.1;++digits;}}
    if(!digits||(*p&&*p!=' '&&*p!='\t'&&*p!=')'))return RF_FORMAT;
    if((float)value!=.25f&&(float)value!=.3f)return RF_FORMAT;
    *out=(float)value;*input=p;return RF_OK;
}
/* Named first-match metadata only. Some installed guns have one clip but TWO
 * radii. Original4c8a10 iterates the clip count; surplus authored radii are
 * retained/validated here, never mistaken for an extra effect. */
static int scene_weapon_impact_visual_profile_read(const void *text,uint32_t bytes,
    const char *name,scene_weapon_impact_visual_profile *out)
{
    const unsigned char *raw=text;scene_weapon_impact_visual_profile value={0};
    uint32_t at=0,fields=0,radii=0;int selected=0;
    if(!text||!name||!out)return RF_RANGE;
    while(at<bytes){char line[1024],*p,*end,label[64];uint32_t n=0;int quote=0,status;
        while(at<bytes&&raw[at]!='\r'&&raw[at]!='\n'){
            if(n+1>=sizeof(line))return RF_RANGE;line[n++]=(char)raw[at++];}
        while(at<bytes&&(raw[at]=='\r'||raw[at]=='\n'))++at;line[n]=0;
        for(p=line;*p;p++){if(*p=='"')quote=!quote;if(!quote&&p[0]=='/'&&p[1]=='/'){*p=0;break;}}
        p=scene_weapon_impact_visual_space(line);end=p+strlen(p);
        while(end>p&&(end[-1]==' '||end[-1]=='\t'))*--end=0;
        if(!strncmp(p,"$Name:",6)){
            if(selected)break;p+=6;status=scene_weapon_impact_visual_string(&p,label);if(status)return status;
            if(*scene_weapon_impact_visual_space(p))return RF_FORMAT;
            selected=scene_weapon_impact_visual_equal(label,name);continue;
        }
        if(!selected)continue;
        if(!strncmp(p,"$Impact Vclips",14)){
            uint32_t is_radius=0,count=0;p=scene_weapon_impact_visual_space(p+14);
            if(!strncmp(p,"Radius:",7)){is_radius=1;p+=7;}
            else if(*p==':')++p;else continue;
            if(fields&(1u<<is_radius))return RF_FORMAT;fields|=1u<<is_radius;
            p=scene_weapon_impact_visual_space(p);if(*p++!='(')return RF_FORMAT;
            for(;;){p=scene_weapon_impact_visual_space(p);if(*p==')'){++p;break;}
                if(count==2)return RF_RANGE;
                if(is_radius){
                    status=scene_weapon_impact_visual_radius(&p,value.radii+count);if(status)return status;
                }else{
                    status=scene_weapon_impact_visual_string(&p,label);if(status)return status;
                    if(scene_weapon_impact_visual_equal(label,"FGatE"))value.kinds[count]=0;
                    else if(scene_weapon_impact_visual_equal(label,"FGatE_lilspark"))value.kinds[count]=1;
                    else if(scene_weapon_impact_visual_equal(label,"FGatE_bigspark"))value.kinds[count]=2;
                    else if(scene_weapon_impact_visual_equal(label,"gatsmoke"))value.kinds[count]=3;
                    else return RF_FORMAT;
                }++count;
            }
            if(*scene_weapon_impact_visual_space(p))return RF_FORMAT;
            if(is_radius)radii=count;else value.count=count;
        }
    }
    if(!selected)return RF_NOT_FOUND;
    if(fields!=3 || !value.count || value.count>radii)return RF_FORMAT;
    /* This bounded family always has one FGatE followed by optional smoke. */
    if(value.kinds[0]>2 || (value.count==2&&value.kinds[1]!=3))return RF_FORMAT;
    *out=value;return RF_OK;
}
/* Reject valid-but-unsupported edited effects before admitting particles to
 * the common simulator. In particular wind/damage cannot fail the frame and
 * swirl cannot consume the shared ambient RNG. Numeric bounds also prevent a
 * cosmetic packet from carrying zero life or overflowing ordinary stepping. */
static uint32_t scene_weapon_impact_visual_particle_supported(const rf_particle_definition *p,
    uint32_t flags,uint32_t emitter)
{
    return p->flags.particle==flags&&p->flags.emitter==emitter&&!p->flags.secondary&&
        isfinite(p->min_life)&&p->min_life>=.001f&&isfinite(p->max_life)&&
        p->max_life>=p->min_life&&p->max_life<=2&&
        isfinite(p->min_radius)&&p->min_radius>=0&&isfinite(p->max_radius)&&
        p->max_radius>=p->min_radius&&p->max_radius<=4&&
        isfinite(p->min_velocity)&&p->min_velocity>=0&&isfinite(p->max_velocity)&&
        p->max_velocity>=p->min_velocity&&p->max_velocity<=64&&
        isfinite(p->direction_random)&&p->direction_random>=-1&&p->direction_random<=1&&
        isfinite(p->growth)&&fabsf(p->growth)<=2&&isfinite(p->acceleration)&&fabsf(p->acceleration)<=10&&
        isfinite(p->gravity_scale)&&fabsf(p->gravity_scale)<=4;
}
static void scene_weapon_impact_visual_resources_close(void)
{
    if(scene_weapon_impact_visual){
        rf_explosion_materials_close(&scene_weapon_impact_visual->materials);
        free(scene_weapon_impact_visual);scene_weapon_impact_visual=NULL;
    }
    rf_scene_weapon_impact_visual[0]=rf_scene_weapon_impact_visual[9]=0;
}
static uint32_t scene_weapon_impact_visual_ready(int32_t weapon)
{return scene_weapon_impact_visual&&weapon>=0&&weapon<64&&scene_weapon_impact_visual->profiles[weapon].count;}
static const rf_particle_animation *scene_weapon_impact_visual_image(uint32_t texture)
{
    if(!scene_weapon_impact_visual||texture>=scene_weapon_impact_visual->materials.count)return NULL;
    return scene_weapon_impact_visual->materials.animations+texture;
}
static void scene_weapon_impact_visual_open(rf_vpp *tables,rf_vpp *maps,uint32_t map_count,
    const rf_weapon_names *weapons)
{
    static const char *const names[]={"FGatE","FGatE_lilspark","FGatE_bigspark"};
    scene_weapon_impact_visual_owner *v=NULL;rf_explosion_definition *combined=NULL;
    rf_vpp_entry entry;unsigned char *text=NULL;uint32_t i,used,peak,fixed;int status=RF_RANGE;
    memset(rf_scene_weapon_impact_visual,0,sizeof(rf_scene_weapon_impact_visual));
    if(scene_weapon_impact_visual||!tables||!maps||!map_count||!weapons||weapons->count>64)goto done;
    fixed=(uint32_t)sizeof(*v)+(uint32_t)sizeof(scene_weapon_impact_visual)+
        (uint32_t)sizeof(rf_scene_weapon_impact_visual)+(uint32_t)sizeof(scene_weapon_impact_visual_names);
    used=fixed+SCENE_WEAPON_IMPACT_VISUAL_SCRATCH;peak=used;
    if(used>=SCENE_WEAPON_IMPACT_VISUAL_BUDGET)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_weapon_impact_visual[11]=rf_xbox_available_memory_pages();
    if((uint64_t)rf_scene_weapon_impact_visual[11]*4096u<
       SCENE_WEAPON_IMPACT_VISUAL_BUDGET+SCENE_WEAPON_IMPACT_VISUAL_HEADROOM){status=RF_NOT_FOUND;goto done;}
#endif
    v=calloc(1,sizeof(*v));if(!v){status=RF_IO;goto done;}
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)goto done;
    if(!entry.size||entry.size>128u*1024u||entry.size>SCENE_WEAPON_IMPACT_VISUAL_BUDGET-used){status=RF_RANGE;goto done;}
    text=malloc(entry.size);if(!text){status=RF_IO;goto done;}
    peak=used+entry.size;status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    for(i=0;i<sizeof(scene_weapon_impact_visual_names)/sizeof(scene_weapon_impact_visual_names[0]);i++){
        int32_t weapon=rf_weapon_name_find(weapons,scene_weapon_impact_visual_names[i]);
        if(weapon<0||weapon>=64){status=RF_FORMAT;goto done;}
        status=scene_weapon_impact_visual_profile_read(text,entry.size,scene_weapon_impact_visual_names[i],v->profiles+weapon);
        if(status)goto done;
    }
    free(text);text=NULL;
    for(i=0;i<3;i++){
        rf_vclip_definition clip;rf_explosion_definition *d=v->definitions+i;
        status=rf_vclip_definition_load(tables,names[i],128u*1024u,&clip);if(status)goto done;
        if(clip.flags!=(i==2?12u:8u)||clip.vfx[0]||clip.has_particle||clip.damage||
           !scene_weapon_impact_visual_equal(clip.explosion,names[i])){status=RF_FORMAT;goto done;}
        status=rf_explosion_definition_load(tables,clip.explosion,128u*1024u,d);if(status)goto done;
        if(d->resolved!=65u||d->recipe.central_count!=1||d->recipe.central[0].process_per_frame||
           d->recipe.central_random!=0||d->recipe.present!=1||d->recipe.play_time!=.2f||
           d->recipe.sparks_count!=(i==0?10:i==1?5:15)||d->emitters[6].spawn_radius!=0||
           d->emitters[0].spawn_radius!=0||d->emitters[0].min_velocity!=0||d->emitters[0].max_velocity!=0||
           d->emitters[6].direction_random< -1||d->emitters[6].direction_random>1||
           !scene_weapon_impact_visual_particle_supported(d->emitters,0x202u,0x16u)||
           !scene_weapon_impact_visual_particle_supported(d->emitters+6,i==2?0x400eu:0x4006u,0x16u)){status=RF_FORMAT;goto done;}
        if(used+128u*1024u>peak)peak=used+128u*1024u;
    }
    status=rf_vclip_definition_load(tables,"gatsmoke",128u*1024u,&v->smoke);if(status)goto done;
    if(v->smoke.flags!=4||!v->smoke.has_particle||v->smoke.particle_count!=5||v->smoke.vfx[0]||
       v->smoke.explosion[0]||v->smoke.vbm[0]||v->smoke.damage||v->smoke.particle.spawn_radius!=0||
       !scene_weapon_impact_visual_particle_supported(&v->smoke.particle,0x4cu,0x10u)){status=RF_FORMAT;goto done;}
    for(i=0;i<3;i++)if(v->smoke.particle.position[i]!=0){status=RF_FORMAT;goto done;}
    combined=calloc(1,sizeof(*combined));if(!combined){status=RF_IO;goto done;}
    used+=(uint32_t)sizeof(*combined);
    for(i=0;i<3;i++){combined->emitters[i*2]=v->definitions[i].emitters[0];combined->emitters[i*2+1]=v->definitions[i].emitters[6];}
    combined->emitters[6]=v->smoke.particle;combined->resolved=127;
    status=rf_explosion_materials_open(&v->materials,combined,maps,map_count,
        SCENE_WEAPON_IMPACT_VISUAL_BUDGET-used);if(status)goto done;
    /* The material owner's inline bytes are already counted in *v. Counting
     * them twice in peak is conservative and reserves allocator overhead. */
    if(used+v->materials.resident_bytes>peak)peak=used+v->materials.resident_bytes;
    v->resident_bytes=fixed+v->materials.resident_bytes-(uint32_t)sizeof(v->materials);
    v->peak_bytes=peak;v->random.value=1;
    scene_weapon_impact_visual=v;v=NULL;
    rf_scene_weapon_impact_visual[0]=1;
    rf_scene_weapon_impact_visual[9]=scene_weapon_impact_visual->resident_bytes;
    rf_scene_weapon_impact_visual[10]=scene_weapon_impact_visual->peak_bytes;
done:
    free(text);free(combined);
    if(v){rf_explosion_materials_close(&v->materials);free(v);}
    rf_scene_weapon_impact_visual[8]=(uint32_t)status;
}
#endif
