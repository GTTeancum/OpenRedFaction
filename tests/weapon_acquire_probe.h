typedef struct wa_fixture {rf_weapon_inventory inventory;rf_weapon_acquire_definition definition;int32_t weapon,quantity;uint32_t fail,calls;} wa_fixture;
static int wa_notify(void *context,rf_weapon_inventory *inventory,uint32_t reason)
{wa_fixture *f=context;if(inventory!=&f->inventory || reason!=1 || inventory->owned[f->weapon]!=1)return RF_FORMAT;++f->calls;return f->fail?RF_IO:RF_OK;}
static int weapon_acquire_probe(void)
{wa_fixture f;int32_t status;
 while(fread(&f,472,1,stdin)==1){f.calls=0;status=rf_weapon_acquire(&f.inventory,&f.definition,f.weapon,f.quantity,wa_notify,&f);if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.inventory,448,1,stdout)!=1 || fwrite(&f.calls,4,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
