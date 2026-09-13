typedef struct wam_fixture {rf_weapon_inventory inventory;rf_weapon_ammo_state state;rf_weapon_acquire_definition definition;int32_t weapon,quantity;uint32_t reloading,fail,trace;} wam_fixture;
static int wam_query(void *context,uint32_t *value){wam_fixture *f=context;f->trace=1;*value=f->reloading;return f->fail==1?RF_IO:RF_OK;}
static int wam_reload(void *context,uint32_t a,uint32_t b){wam_fixture *f=context;if(a || b)return RF_FORMAT;f->trace=f->trace*10+2;return f->fail==2?RF_IO:RF_OK;}
static int weapon_ammo_probe(void)
{wam_fixture f;int32_t status;
 while(fread(&f,488,1,stdin)==1){rf_weapon_ammo_backend b={&f,wam_query,wam_reload};f.trace=0;status=rf_weapon_add_ammo(&f.inventory,&f.state,&f.definition,f.weapon,f.quantity,&b);if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.inventory,460,1,stdout)!=1 || fwrite(&f.trace,4,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
