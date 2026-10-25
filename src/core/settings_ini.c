#include "rf/settings_ini.h"
#include <string.h>
void rf_settings_defaults(rf_settings *s)
{
    memset(s,0,sizeof(*s));
    s->frame_rate=RF_FRAME_RATE_60;s->fallback=RF_FRAME_RATE_30_SMOOTH;s->smooth_characters=1;s->flicker_filter=UINT32_MAX;s->compressed_textures=1;
}
static int lower(int c){return c>='A' && c<='Z'?c-'A'+'a':c;}
static int same(const char *a,uint32_t n,const char *b)
{
    uint32_t i;
    for(i=0;i<n && b[i];i++)if(lower((unsigned char)a[i])!=b[i])return 0;
    return i==n && !b[i];
}
static void trim(const char **p,uint32_t *n)
{
    while(*n && ((*p)[0]==' ' || (*p)[0]=='\t')){++*p;--*n;}
    while(*n && ((*p)[*n-1]==' ' || (*p)[*n-1]=='\t' || (*p)[*n-1]=='\r'))--*n;
}
void rf_settings_parse(rf_settings *s,const char *text,uint32_t bytes)
{
    uint32_t at=0;
    while(at<bytes) {
        const char *line=text+at,*key,*value;uint32_t n=0,k,kn,vn;
        while(at+n<bytes && line[n]!='\n')n++;
        at+=n+(at+n<bytes);
        for(k=0;k<n;k++)if(line[k]==';' || line[k]=='#')break;
        n=k;trim(&line,&n);
        if(!n || line[0]=='[')continue;
        for(k=0;k<n && line[k]!='=';k++){}
        if(k==n){++s->unknown;continue;}
        key=line;kn=k;value=line+k+1;vn=n-k-1;trim(&key,&kn);trim(&value,&vn);
        if(same(key,kn,"frame_rate")) {
            if(same(value,vn,"60"))s->frame_rate=RF_FRAME_RATE_60;
            else if(same(value,vn,"30"))s->frame_rate=RF_FRAME_RATE_30;
            else if(same(value,vn,"30_smooth"))s->frame_rate=RF_FRAME_RATE_30_SMOOTH;
            else if(same(value,vn,"auto"))s->frame_rate=RF_FRAME_RATE_AUTO;
            else ++s->invalid;
        } else if(same(key,kn,"fallback")) {
            if(same(value,vn,"30_smooth"))s->fallback=RF_FRAME_RATE_30_SMOOTH;
            else if(same(value,vn,"30"))s->fallback=RF_FRAME_RATE_30;
            else if(same(value,vn,"none"))s->fallback=RF_FRAME_RATE_60;
            else ++s->invalid;
        } else if(same(key,kn,"smooth_characters")) {
            if(same(value,vn,"on"))s->smooth_characters=1;
            else if(same(value,vn,"off"))s->smooth_characters=0;
            else ++s->invalid;
        } else if(same(key,kn,"textures")) {
            if(same(value,vn,"compressed"))s->compressed_textures=1;
            else if(same(value,vn,"original"))s->compressed_textures=0;
            else ++s->invalid;
        } else if(same(key,kn,"flicker_filter")) {
            if(same(value,vn,"auto"))s->flicker_filter=UINT32_MAX;
            else if(vn==1 && value[0]>='0' && value[0]<='5')s->flicker_filter=(uint32_t)(value[0]-'0');
            else ++s->invalid;
        } else ++s->unknown;
    }
}
