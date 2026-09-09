#include "rf/entity_assets.h"
#include <string.h>
int rf_entity_skeletal_filename(const char *authored,char compiled[64])
{
    uint32_t length=0,stem=0;int dot=0;
    if(!authored || !compiled)return RF_RANGE;
    while(length<64 && authored[length]) {
        if(authored[length]=='.') {stem=length;dot=1;}
        ++length;
    }
    if(length==64)return RF_RANGE;
    if(!dot)stem=length;
    if(stem>59)return RF_RANGE;
    memmove(compiled,authored,stem);memcpy(compiled+stem,".v3c",5);
    return RF_OK;
}
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
static int asset(char destination[64],const char *source)
{size_t n=strlen(source);if(!n || n>=64)return RF_RANGE;memcpy(destination,source,n+1);return RF_OK;}
int rf_entity_assets_read(const void *text,uint32_t bytes,const char *class_name,const char *skin,rf_entity_assets *assets)
{
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_assets value={0};char t[256];
    int quoted,status,selected=0,found=0,skin_found;
    if(!text || !class_name || !*class_name || !skin || !assets)return RF_RANGE;
    skin_found=!*skin;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            selected=same(t,class_name);found=selected;
        } else if(selected && same(t,"$V3D")) {
            status=token(&l,t,&quoted);if(status || quoted || !same(t,"Filename:"))return RF_FORMAT;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            if(*t) {status=asset(value.model,t);if(status)return status;}else value.model[0]=0;
        } else if(selected && same(t,"$Skin:")) {
            int use;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;use=*skin && same(t,skin);
            status=token(&l,t,&quoted);if(status || quoted || strcmp(t,"("))return RF_FORMAT;
            if(use && skin_found)return RF_FORMAT;
            for(;;) {
                status=token(&l,t,&quoted);if(status)return RF_FORMAT;
                if(!quoted && !strcmp(t,")"))break;
                if(!quoted)return RF_FORMAT;
                if(use) {if(value.texture_count==64)return RF_RANGE;status=asset(value.textures[value.texture_count++],t);if(status)return status;}
            }
            if(use)skin_found=1;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found || !skin_found)return RF_NOT_FOUND;
    *assets=value;return RF_OK;
}
