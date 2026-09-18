#ifndef RF_SCENE_PICKUP_CLASS_NAMES_INC
#define RF_SCENE_PICKUP_CLASS_NAMES_INC
/* Installed items.tbl names; existing indices0..10 remain stable.
 * Slots match the shared implemented weapon list, not table weapon IDs. */
enum { SCENE_PICKUP_FIRST_AID=10, SCENE_PICKUP_CLASSES=20 };
static const char *pickup_classes[SCENE_PICKUP_CLASSES]={
    "Handgun","Medical Kit","Suit Repair","12mm_ammo","Assault Rifle",
    "5.56mm_ammo","Riot Stick","riot_stick_battery","Shotgun","10gauge_ammo","First Aid Kit",
    "Remote Charge","Remote Charges","Sniper Rifle","rocket launcher","grenades",
    "rail gun",".50cal_ammo","rocket_launcher_ammo","railgun_bolts"
};
static int scene_pickup_name_equal(const char *a,const char *b)
{
    unsigned char x,y;if(!a || !b)return 0;
    do{x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;
    }while(x);return 1;
}
static int pickup_class(const char *name)
{
    int i;for(i=0;i<SCENE_PICKUP_CLASSES;i++)if(scene_pickup_name_equal(name,pickup_classes[i]))return i;
    return -1;
}
static int scene_pickup_weapon_slot(int kind)
{
    static const signed char slots[SCENE_PICKUP_CLASSES]={0,-1,-1,0,1,1,2,2,3,3,-1,8,8,6,4,5,7,6,4,7};
    return kind>=0 && kind<SCENE_PICKUP_CLASSES?slots[kind]:-1;
}
#endif
