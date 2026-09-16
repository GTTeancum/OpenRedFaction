#include "rf/clutter_gameplay.h"
#include "rf/clutter.h"
#include <string.h>
#include <math.h>
#include <float.h>
/* Private lexer/decimal routines mirror entity_assets.c because those helpers
 * are private. In particular never call NXDK strtod/strtof assertion stubs. */
typedef struct lexer {const unsigned char *text;uint32_t size,at;} lexer;
static int same(const char *a,const char *b)
{
    while(*a && *b) {unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;if(x!=y)return 0;}
    return *a==*b;
}
static int token(lexer *l,char out[256],int *quoted)
{
    uint32_t n=0;unsigned c;
    for(;;) {
        while(l->at<l->size && l->text[l->at]<=32) {if(!l->text[l->at])return RF_FORMAT;++l->at;}
        if(l->at==l->size)return RF_NOT_FOUND;
        if(l->at+1<l->size && l->text[l->at]=='/' && l->text[l->at+1]=='/') {
            while(l->at<l->size && l->text[l->at]!='\n')++l->at;
        } else break;
    }
    *quoted=l->text[l->at]=='"';
    if(*quoted) {
        ++l->at;
        while(l->at<l->size && l->text[l->at]!='"') {
            c=l->text[l->at++];if(!c || c=='\r' || c=='\n')return RF_FORMAT;
            if(n==255)return RF_RANGE;out[n++]=(char)c;
        }
        if(l->at==l->size)return RF_FORMAT;++l->at;
    } else if(strchr("(){}",l->text[l->at]))out[n++]=(char)l->text[l->at++];
    else while(l->at<l->size && l->text[l->at]>32 && !strchr("(){}\"",l->text[l->at])) {
        if(l->at+1<l->size && l->text[l->at]=='/' && l->text[l->at+1]=='/')break;
        if(n==255)return RF_RANGE;out[n++]=(char)l->text[l->at++];
    }
    out[n]=0;return RF_OK;
}
static int sphere_number(lexer *l,float *result)
{
    char t[256];uint32_t at=0,digits=0;int quoted,status,negative=0,fraction=0,exponent=0,exp_negative=0;double value=0;
    status=token(l,t,&quoted);if(status || quoted)return RF_FORMAT;
    /* NXDK strtod/strtof are assertion stubs. Authored decimal syntax only. */
    if(t[at]=='+' || t[at]=='-')negative=t[at++]=='-';
    while(t[at]>='0' && t[at]<='9') {value=value*10+(t[at++]-'0');++digits;}
    if(t[at]=='.') {
        ++at;while(t[at]>='0' && t[at]<='9') {value=value*10+(t[at++]-'0');++digits;++fraction;}
    }
    if(!digits)return RF_FORMAT;
    if(t[at]=='e' || t[at]=='E') {
        ++at;if(t[at]=='+' || t[at]=='-')exp_negative=t[at++]=='-';
        digits=0;while(t[at]>='0' && t[at]<='9') {if(exponent<10000)exponent=exponent*10+t[at]-'0';++at;++digits;}
        if(!digits)return RF_FORMAT;
    }
    if(t[at])return RF_FORMAT;
    exponent=(exp_negative?-exponent:exponent)-fraction;
    if(value!=0) {
        if(exponent>308)return RF_FORMAT;
        if(exponent< -600)value=0;
        else {
            while(exponent>0) {value*=10;--exponent;}
            while(exponent<0) {value/=10;++exponent;}
        }
    }
    if(!isfinite(value) || value>FLT_MAX)return RF_FORMAT;
    *result=(float)(negative?-value:value);return RF_OK;
}
static int metadata_tag(lexer *l,const char *tag)
{
    lexer probe=*l;char expected[64],actual[256];uint32_t n;int quoted;
    while(*tag) {
        n=0;while(*tag && *tag!=' ')expected[n++]=*tag++;
        expected[n]=0;while(*tag==' ')++tag;
        if(token(&probe,actual,&quoted) || quoted || !same(actual,expected))return 0;
    }
    *l=probe;return 1;
}

static int gameplay_string(lexer *l,char out[64])
{
    char t[256];int quoted,status=token(l,t,&quoted);
    if(status || !quoted)return RF_FORMAT;
    if(strlen(t)>=64)return RF_RANGE;
    strcpy(out,t);return RF_OK;
}
/* Original5129d0 requires angle brackets and accepts optional commas. */
static int gameplay_vector(lexer *l,float out[3])
{
    uint32_t i,start;lexer component;
    while(l->at<l->size && l->text[l->at]<=32)++l->at;
    if(l->at==l->size || l->text[l->at++]!='<')return RF_FORMAT;
    for(i=0;i<3;++i){
        while(l->at<l->size && l->text[l->at]<=32)++l->at;
        start=l->at;
        while(l->at<l->size && l->text[l->at]>32 &&
              l->text[l->at]!=',' && l->text[l->at]!='>')++l->at;
        component=(lexer){l->text+start,l->at-start,0};
        if(sphere_number(&component,&out[i]))return RF_FORMAT;
        while(l->at<l->size && l->text[l->at]<=32)++l->at;
        if(i<2 && l->at<l->size && l->text[l->at]==',')++l->at;
    }
    if(l->at==l->size || l->text[l->at++]!='>')return RF_FORMAT;
    return RF_OK;
}
int rf_clutter_gameplay_read(const void *text,uint32_t bytes,const char *name,
    rf_clutter_gameplay_definition *result)
{
    static const char *factor_names[]={"bash","bullet","armor piercing bullet","explosive","fire","energy","electrical","acid","scalding"};
    rf_clutter_definition base;rf_clutter_gameplay_definition value={0};
    lexer l={(const unsigned char*)text,bytes,0};char t[256];
    uint32_t i,seen=0,bit;int status,quoted,found=0;
    if(!text || !name || !*name || !result)return RF_RANGE;
    status=rf_clutter_definition_read(text,bytes,name,&base);if(status)return status;
    strcpy(value.name,base.name);strcpy(value.explosion,base.explosion);strcpy(value.corpse,base.corpse);
    value.life=base.life;value.flags=base.flags;value.protected_object=base.life<0;
    value.explosion_radius=value.explosion_damage=1;
    for(i=0;i<11;++i)value.damage_factors[i]=1;
    while((status=token(&l,t,&quoted))==RF_OK){
        if(quoted)continue;
        if(same(t,"$Class") && metadata_tag(&l,"Name:")){
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,name);continue;
        }
        if(!found)continue;
        if(same(t,"$Skin:"))break;
        bit=0;
        if(same(t,"$Damage")){
            if(!metadata_tag(&l,"Type Factor:"))return RF_FORMAT;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            for(i=0;i<9;++i)if(same(t,factor_names[i]))break;
            if(i==9 || sphere_number(&l,&value.damage_factors[i]))return RF_FORMAT;
        }else if(same(t,"$Explode")){
            if(metadata_tag(&l,"Anim Radius:")){bit=1;if(sphere_number(&l,&value.explosion_radius))return RF_FORMAT;}
            else if(metadata_tag(&l,"Damage:")){bit=2;if(sphere_number(&l,&value.explosion_damage))return RF_FORMAT;}
            else if(metadata_tag(&l,"Offset:")){bit=4;if(gameplay_vector(&l,value.explosion_offset))return RF_FORMAT;}
        }else if(same(t,"$Debris")){
            if(metadata_tag(&l,"Velocity:")){bit=8;if(sphere_number(&l,&value.debris_velocity))return RF_FORMAT;value.present|=RF_CLUTTER_GAMEPLAY_DEBRIS_VELOCITY;}
            else if(metadata_tag(&l,"Filename:")){bit=16;status=gameplay_string(&l,value.debris_model);if(status)return status;value.present|=RF_CLUTTER_GAMEPLAY_DEBRIS_MODEL;}
            else if(metadata_tag(&l,"Sound Set:")){bit=32;status=gameplay_string(&l,value.debris_sound);if(status)return status;value.present|=RF_CLUTTER_GAMEPLAY_DEBRIS_SOUND;}
        }
        if(seen&bit)return RF_FORMAT;
        seen|=bit;
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    *result=value;return RF_OK;
}
