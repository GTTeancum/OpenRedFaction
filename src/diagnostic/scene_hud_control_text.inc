/* Authored training messages contain action tokens. Expand only presentation
 * text using the current platform's real bindings; never alter the stored
 * subtitle, its timing length, voice owner or gameplay input. */
uint32_t rf_hud_control_text_diagnostic[8]; /* calls,bound tokens,unbound tokens,unknown tokens,failures,output bytes,message UID,status */
static int scene_hud_control_text(const char *source,uint32_t source_capacity,
    char *output,uint32_t capacity,uint32_t *bound,uint32_t *unbound,uint32_t *unknown,uint32_t *bytes)
{
#define SCENE_CONTROL_LABEL(token,label,mapped) {token,label,sizeof(token)-1,sizeof(label)-1,mapped}
    static const struct {const char *token,*label;uint32_t token_bytes,label_bytes,mapped;} bindings[]={
#ifdef RF_IMAGE_XBOX_NATIVE
        SCENE_CONTROL_LABEL("$USE$","X",1),SCENE_CONTROL_LABEL("$FIRE$","RT",1),SCENE_CONTROL_LABEL("$ALT_FIRE$","LT",1),
        SCENE_CONTROL_LABEL("$JUMP$","A",1),SCENE_CONTROL_LABEL("$CROUCH$","B",1),SCENE_CONTROL_LABEL("$RELOAD$","Y",1),
        SCENE_CONTROL_LABEL("$NEXT_WEAPON$","D-PAD RIGHT",1),SCENE_CONTROL_LABEL("$PREV_WEAPON$","D-PAD LEFT",1),
#else
        /* Existing PC keyboard/mouse defaults; no PC runtime claim. */
        SCENE_CONTROL_LABEL("$USE$","E",1),SCENE_CONTROL_LABEL("$FIRE$","MOUSE1 / F",1),SCENE_CONTROL_LABEL("$ALT_FIRE$","MOUSE2 / G",1),
        SCENE_CONTROL_LABEL("$JUMP$","SPACE",1),SCENE_CONTROL_LABEL("$CROUCH$","CTRL",1),SCENE_CONTROL_LABEL("$RELOAD$","R",1),
        SCENE_CONTROL_LABEL("$NEXT_WEAPON$","TAB",1),SCENE_CONTROL_LABEL("$PREV_WEAPON$","Q",1),
#endif
        /* Present in train02, but no corresponding input field/binding exists
         * in this first-playable frontend. Never invent a controller button. */
        SCENE_CONTROL_LABEL("$HOLSTER$","UNBOUND",0),SCENE_CONTROL_LABEL("$MESSAGE_LOG$","UNBOUND",0),SCENE_CONTROL_LABEL("$FINE_AIM$","UNBOUND",0)
    };
#undef SCENE_CONTROL_LABEL
    uint32_t i=0,at=0,b=0,u=0,other=0;
    if(!source||!output||!source_capacity||!capacity||!bound||!unbound||!unknown||!bytes)return RF_RANGE;
    while(i<source_capacity&&source[i]){
        const char *replacement=source+i;uint32_t read=1,write=1,j;
        if(source[i]=='$'){
            for(j=0;j<sizeof(bindings)/sizeof(bindings[0]);j++){
                uint32_t size=bindings[j].token_bytes;
                if(size<=source_capacity-i&&!memcmp(source+i,bindings[j].token,size)){
                    replacement=bindings[j].label;read=size;write=bindings[j].label_bytes;
                    if(bindings[j].mapped)++b;else ++u;break;
                }
            }
            if(j==sizeof(bindings)/sizeof(bindings[0])){
                /* Preserve unknown tokens verbatim, including both delimiters.
                 * A currency/literal dollar without a token stays one byte. */
                uint32_t end=i+1;
                while(end<source_capacity&&((source[end]>='A'&&source[end]<='Z')||
                      (source[end]>='0'&&source[end]<='9')||source[end]=='_'))++end;
                if(end<source_capacity&&end>i+1&&source[end]=='$'){
                    read=write=end-i+1;++other;
                }
            }
        }
        if(write>=capacity-at)return RF_RANGE;
        if(write==1)output[at]=replacement[0];else memcpy(output+at,replacement,write);
        at+=write;i+=read;
    }
    if(i==source_capacity)return RF_RANGE;
    output[at]=0;*bound=b;*unbound=u;*unknown=other;*bytes=at;return RF_OK;
}
