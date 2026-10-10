#ifndef RF_WEAPON_IMPACT_AUDIO_READER_INCLUDED
#define RF_WEAPON_IMPACT_AUDIO_READER_INCLUDED
/* Include once in entity_assets.c, after lexer/token/metadata_string/same.
 * 4c2b80..4c2b95 initializes both ten-entry impact tables to-1.
 * 4c3e26..4c3f1d parses repeated material/Foley pairs. The underwater
 * suffix goes to1a0; ordinary pairs go to178. 4686c0 maps unknown names0.
 * This reader retains metadata only and never registers or loads sounds. */
#include "rf/weapon_impact_audio.h"

static int weapon_impact_audio_material(char name[64],uint32_t *bank,uint32_t *material)
{
    size_t length=strlen(name),i;uint32_t underwater=0;
    if(length>=11 && same(name+length-11," underwater")) {
        name[length-11]=0;underwater=1;
    } else {
        /* The original searches for the word then removes the last11 bytes.
         * Only the authored suffix has a supported meaning in this reader. */
        for(i=0;i+10<=length;i++) {
            char word[11];memcpy(word,name+i,10);word[10]=0;
            if(same(word,"underwater"))return RF_FORMAT;
        }
    }
    *bank=underwater;*material=rf_clutter_material_index(name);return RF_OK;
}

int rf_weapon_impact_audio_groups_read(const void *text,uint32_t bytes,
    const rf_weapon_names *names,const rf_foley_owner *sounds,
    rf_weapon_impact_audio_groups *result)
{
    lexer l={text,bytes,0};char token_text[256],name[64],material_name[64];
    rf_weapon_impact_audio_groups *value;uint32_t index=0,i,bank,material;
    int status,quoted,active=0;int32_t group;
    if(!text || !bytes || !names || names->count>64 || !sounds || !result)return RF_RANGE;
    for(i=0;i<names->count;i++)
        if(!memchr(names->names[i],0,sizeof(names->names[i])))return RF_FORMAT;
    /* Validate the borrowed Foley envelope even when every impact is silent. */
    status=rf_foley_find(sounds,"",&group);if(status)return status;
    value=malloc(sizeof(*value));if(!value)return RF_RANGE;
    memset(value->groups,0xff,sizeof(value->groups));value->count=names->count;
    while((status=token(&l,token_text,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(token_text,"$Name:")) {
            if(index>=names->count){status=RF_FORMAT;goto done;}
            status=metadata_string(&l,name,sizeof(name));if(status)goto done;
            if(!same(name,names->names[index])){status=RF_FORMAT;goto done;}
            ++index;active=1;
        } else if(same(token_text,"#End"))active=0;
        else if(active && same(token_text,"$Impact")) {
            if(token(&l,token_text,&quoted) || quoted){status=RF_FORMAT;goto done;}
            if(!same(token_text,"Sound:"))continue;
            status=metadata_string(&l,material_name,sizeof(material_name));if(status)goto done;
            status=metadata_string(&l,name,sizeof(name));if(status)goto done;
            status=weapon_impact_audio_material(material_name,&bank,&material);if(status)goto done;
            status=rf_foley_find(sounds,name,&group);if(status)goto done;
            value->groups[index-1][bank][material]=group;
        }
    }
    if(status==RF_NOT_FOUND) {
        if(index!=names->count)status=RF_FORMAT;
        else {*result=*value;status=RF_OK;}
    }
done:
    free(value);return status;
}

int rf_weapon_impact_audio_groups_load(rf_vpp *tables,uint32_t budget,
    const rf_weapon_names *names,const rf_foley_owner *sounds,
    rf_weapon_impact_audio_groups *result)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !names || !sounds || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size || (uint64_t)entry.size+sizeof(*result)>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_weapon_impact_audio_groups_read(text,entry.size,names,sounds,result);
    free(text);return status;
}
#endif
