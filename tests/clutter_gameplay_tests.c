#include "rf/clutter_gameplay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define BASE "$Class Name: \"lamp\"\n$V3D Filename: \"lamp.v3d\"\n$Material: \"glass\"\n$Life: 80\n$Flags: (\"collide_weapon\")\n"
static int failed(const char *text,const char *name,int expected)
{
    rf_clutter_gameplay_definition d,before;
    memset(&d,0xa5,sizeof(d));before=d;
    CHECK(rf_clutter_gameplay_read(text,(uint32_t)strlen(text),name,&d)==expected);
    CHECK(!memcmp(&d,&before,sizeof(d)));return 0;
}
static int installed(const char *path)
{
    const char *names[]={"lantern_box","lantern_ball","chinese_lamp"};
    rf_vpp a;rf_vpp_entry e;void *text;unsigned i;rf_clutter_gameplay_definition d;
    CHECK(rf_vpp_open(&a,path)==RF_OK);CHECK(rf_vpp_find(&a,"clutter.tbl",&e)==RF_OK);
    CHECK(e.size>0 && e.size<=2*1024*1024);text=malloc(e.size);CHECK(text);
    CHECK(rf_vpp_read(&a,&e,0,text,e.size)==RF_OK);
    for(i=0;i<3;++i){
        CHECK(rf_clutter_gameplay_read(text,e.size,names[i],&d)==RF_OK);
        CHECK(d.life==80 && !d.protected_object && !strcmp(d.explosion,"yellboom"));
        CHECK(d.explosion_radius==(i==0?.2f:.4f));
        CHECK(d.present==7 && d.debris_velocity==3 && !d.corpse[0]);
        printf("installed %s: life %.0f, debris %.0f, effect %s\n",d.name,d.life,d.debris_velocity,d.explosion);
    }
    free(text);rf_vpp_close(&a);return 0;
}
int main(int argc,char **argv)
{
    rf_clutter_gameplay_definition d;char text[2048];unsigned i;
    CHECK(rf_clutter_gameplay_read(BASE,sizeof(BASE)-1,"LAMP",&d)==RF_OK);
    CHECK(d.life==80 && !d.protected_object && !d.present);
    CHECK(d.explosion_radius==1 && d.explosion_damage==1);
    for(i=0;i<11;++i)CHECK(d.damage_factors[i]==1);
    CHECK(rf_clutter_gameplay_read(BASE "$Explode Offset: <1 2 3>",sizeof(BASE "$Explode Offset: <1 2 3>")-1,"lamp",&d)==RF_OK);
    CHECK(d.explosion_offset[2]==3);
    CHECK(!failed(BASE,"absent",RF_NOT_FOUND));
    CHECK(!failed("$Name: \"lamp\"\n$Life: 1\n","lamp",RF_NOT_FOUND));
    CHECK(!failed(BASE "$Life: 2\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Damage Type Factor: \"unknown\" 1\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Damage Type Factor: \"fire\" nan\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Damage Type Factor: \"fire\" 1e400\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Damage Type Factor: \"fire\" 1e\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Damage Type Broken: \"fire\" 1\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Explode Anim Radius: 1\n$Explode Anim Radius: 2\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Explode Offset: <1,2>\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Explode Offset: <1,2,3,4>\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Explode Offset: <1,2,inf>\n","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Debris Filename: \"unterminated","lamp",RF_FORMAT));
    CHECK(!failed(BASE "$Debris Filename: \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"\n","lamp",RF_RANGE));
    CHECK(!failed(BASE "$Debris Velocity: 1\n$Debris Velocity: 2\n","lamp",RF_FORMAT));
    strcpy(text,BASE "$Damage Type Factor: \"FIRE\" -.5\n$Damage Type Factor: \"fire\" 2e-1\n"
        "$Explode Anim: \"missing-resource\"\n$Corpse Class Name: \"missing-class\"\n"
        "$Explode Offset: <1,-2,.5>\n$Debris Velocity: 3\n$Debris Filename: \"\"\n$Debris Sound Set: \"bounce\"\n"
        "$Skin: \"other\"\n$Debris Velocity: nan\n$Class Name: \"lamp\"\n$Life: -100\n");
    CHECK(rf_clutter_gameplay_read(text,(uint32_t)strlen(text),"lamp",&d)==RF_OK);
    CHECK(d.damage_factors[4]==.2f && d.damage_factors[9]==1 && d.damage_factors[10]==1);
    CHECK(d.explosion_offset[0]==1 && d.explosion_offset[1]==-2 && d.explosion_offset[2]==.5f);
    CHECK(d.present==7 && d.debris_velocity==3 && !d.debris_model[0]);
    CHECK(!strcmp(d.explosion,"missing-resource") && !strcmp(d.corpse,"missing-class"));
    /* Resource absence is a binder policy, never a parser lookup. */
    for(i=0;i<2;++i){
        snprintf(text,sizeof(text),"$Class Name: \"lamp\" $V3D Filename: \"m\" $Material: \"glass\" $Flags: () $Life: %d",i?-1:0);
        CHECK(rf_clutter_gameplay_read(text,(uint32_t)strlen(text),"lamp",&d)==RF_OK);
        CHECK(d.life==(i?-1:0) && d.protected_object==i);
    }
    strcpy(text,BASE);text[10]=0;
    {rf_clutter_gameplay_definition before;memset(&d,0xa5,sizeof(d));before=d;
     CHECK(rf_clutter_gameplay_read(text,sizeof(BASE)-1,"lamp",&d)==RF_FORMAT);CHECK(!memcmp(&d,&before,sizeof(d)));}
    if(argc==2)CHECK(!installed(argv[1]));else CHECK(argc==1);
    puts("clutter gameplay parser cases passed");return 0;
}
