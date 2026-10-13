/* Bounded strings.tbl UI subset; no retained archive, font or scene borrower.
 * Only the current language is resident. Tables and campaign placeholders are
 * installed source evidence; port-specific prompts deliberately remain English. */
#define SCENE_TEXT_LABEL_BYTES 64u
static const uint32_t scene_text_ids[]={42,43,986,788,85,86,87,88,123,138,143,135,
    124,156,983,145,146,147,148,149,150,151,152,153,154,169,170,202,820,821,590,59};
#define SCENE_TEXT_COUNT (sizeof(scene_text_ids)/sizeof(scene_text_ids[0]))
static char scene_text_labels[SCENE_TEXT_COUNT][SCENE_TEXT_LABEL_BYTES];
static uint32_t scene_text_language;
static const char *scene_text(uint32_t id,const char *fallback)
{
    uint32_t i;for(i=0;i<SCENE_TEXT_COUNT;i++)if(scene_text_ids[i]==id)
        return scene_text_labels[i][0]?scene_text_labels[i]:fallback;
    return fallback;
}
static const char *scene_text_language_name(void)
{
    return scene_text_language==RF_TEXT_GERMAN?"DEUTSCH":
        scene_text_language==RF_TEXT_FRENCH?"FRANCAIS":"ENGLISH";
}
typedef struct scene_text_reader {
    rf_vpp *archive;rf_vpp_entry *entry;uint32_t position,at,count;unsigned char bytes[512];
} scene_text_reader;
static int scene_text_next(scene_text_reader *r,unsigned char *out)
{
    if(r->at==r->count){
        int status;if(r->position==r->entry->size)return RF_NOT_FOUND;
        r->count=r->entry->size-r->position;if(r->count>sizeof(r->bytes))r->count=sizeof(r->bytes);
        status=rf_vpp_read(r->archive,r->entry,r->position,r->bytes,r->count);if(status)return status;
        r->position+=r->count;r->at=0;
    }
    *out=r->bytes[r->at++];return RF_OK;
}
/* Bounds reflect the existing fixed-cell menu rectangles, including values
 * and the longest physical-button label. Oversize records use English rather
 * than truncating a label or drawing outside its row. */
static uint32_t scene_text_label_limit(uint32_t id)
{
    if(id>=145 && id<=154)return 17;
    if(id==169 || id==170)return 9;
    return 24;
}
static int scene_text_quoted(const char *p,char out[SCENE_TEXT_LABEL_BYTES])
{
    uint32_t n=0;while(*p==' ' || *p=='\t')++p;if(*p++!='"')return RF_FORMAT;
    while(*p && *p!='"'){
        unsigned char c=(unsigned char)*p++;
        if(c=='\\'){
            c=(unsigned char)*p++;if(!c)return RF_FORMAT;
            if(c=='n' || c=='r' || c=='t')c=' ';
            else if(c!='"' && c!='\\')return RF_FORMAT;
        }
        if(c<32 || c==127 || c>=253 || n+1>=SCENE_TEXT_LABEL_BYTES)return RF_RANGE;
        out[n++]=(char)c;
    }
    if(*p++!='"')return RF_FORMAT;
    while(*p==' ' || *p=='\t' || *p=='\r')++p;
    if(*p && !(p[0]=='/' && p[1]=='/'))return RF_FORMAT;
    out[n]=0;return RF_OK;
}
int rf_scene_text_configure(const char *tables_path,uint32_t language)
{
    struct candidate {char labels[SCENE_TEXT_COUNT][SCENE_TEXT_LABEL_BYTES];uint32_t rank[SCENE_TEXT_COUNT];} *next=NULL;
    rf_vpp archive={0};rf_vpp_entry entry;scene_text_reader reader={0};char line[1024];
    uint32_t used=0,overflow=0,current=UINT32_MAX,i;int status;
    if(!tables_path || language>=RF_TEXT_LANGUAGE_COUNT)return RF_RANGE;
    /* A failed optional catalog must not leave the previous language visible. */
    scene_text_language=language;memset(scene_text_labels,0,sizeof(scene_text_labels));
    status=rf_vpp_open(&archive,tables_path);if(status)return status;
    status=rf_vpp_find(&archive,"strings.tbl",&entry);if(status)goto done;
    if(!entry.size || entry.size>131072u){status=RF_RANGE;goto done;}
    next=calloc(1,sizeof(*next));if(!next){status=RF_IO;goto done;}
    reader.archive=&archive;reader.entry=&entry;
    for(;;){
        unsigned char c;status=scene_text_next(&reader,&c);
        if(status && status!=RF_NOT_FOUND)goto done;
        if(!status && c!='\n'){
            if(!c){status=RF_FORMAT;goto done;}
            if(used+1<sizeof(line))line[used++]=(char)c;else overflow=1;
            continue;
        }
        line[used]=0;
        if(!overflow){
            char *p=line;while(*p==' ' || *p=='\t')++p;
            if(*p=='#'){
                uint32_t id=0;current=UINT32_MAX;++p;
                if(*p>='0' && *p<='9'){
                    while(*p>='0' && *p<='9'){
                        uint32_t digit=(uint32_t)(*p++-'0');
                        if(id>(UINT32_MAX-digit)/10u){status=RF_RANGE;goto done;}id=id*10u+digit;
                    }
                    while(*p==' ' || *p=='\t' || *p=='\r')++p;
                    if(!*p)for(i=0;i<SCENE_TEXT_COUNT;i++)if(scene_text_ids[i]==id){current=i;break;}
                }
            }else if(current<SCENE_TEXT_COUNT && strlen(p)>=3 && p[2]==':'){
                uint32_t lang=rf_text_tag_language(p),rank=lang==language?2u:lang==RF_TEXT_ENGLISH?1u:0u;
                char value[SCENE_TEXT_LABEL_BYTES];
                if(rank && !scene_text_quoted(p+3,value) &&
                   strlen(value)<=scene_text_label_limit(scene_text_ids[current]) &&
                   (lang==RF_TEXT_ENGLISH || rf_text_translation_available(value,(uint32_t)strlen(value))) &&
                   rank>next->rank[current]){
                    strcpy(next->labels[current],value);next->rank[current]=rank;
                }
            }
        }else current=UINT32_MAX; /* Never attach later fields to an oversized header. */
        used=overflow=0;if(status==RF_NOT_FOUND)break;
    }
    memcpy(scene_text_labels,next->labels,sizeof(scene_text_labels));status=RF_OK;
done:
    free(next);rf_vpp_close(&archive);return status;
}
