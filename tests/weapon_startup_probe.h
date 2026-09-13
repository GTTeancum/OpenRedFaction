typedef struct weapon_startup_fixture {rf_weapon_inventory inventory;rf_weapon_startup_state state;int32_t defaults[3];rf_weapon_acquire_definition definitions[64];int32_t after_primary;uint32_t fail,calls;} weapon_startup_fixture;
static int weapon_startup_equip(void *context,int32_t weapon){weapon_startup_fixture *f=context;if(weapon!=f->state.primary || weapon!=f->defaults[0])return RF_FORMAT;++f->calls;f->defaults[0]=f->after_primary;return f->fail?RF_IO:RF_OK;}
static int weapon_startup_probe(void)
{weapon_startup_fixture f;int32_t status;
 while(fread(&f,1244,1,stdin)==1){f.calls=0;status=rf_weapon_startup_grant_sp(&f.inventory,&f.state,f.defaults,f.definitions,weapon_startup_equip,&f);if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.inventory,468,1,stdout)!=1 || fwrite(&f.calls,4,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
