#ifndef RF_TEXT_LANGUAGE_H
#define RF_TEXT_LANGUAGE_H
#include <stdint.h>
/* Authored single-byte table languages. This is not voice/asset-pack selection. */
enum {RF_TEXT_ENGLISH,RF_TEXT_GERMAN,RF_TEXT_FRENCH,RF_TEXT_LANGUAGE_COUNT};
static inline uint32_t rf_text_tag_language(const char *p)
{
    unsigned char a=(unsigned char)p[0],b=(unsigned char)p[1];
    if(a>='A' && a<='Z')a+=32;if(b>='A' && b<='Z')b+=32;
    return a=='e' && b=='n'?RF_TEXT_ENGLISH:a=='g' && b=='r'?RF_TEXT_GERMAN:
        a=='f' && b=='r'?RF_TEXT_FRENCH:RF_TEXT_LANGUAGE_COUNT;
}
/* Installed untranslated records include malformed "(fr..." prefixes as well
 * as "(fr)"/"(gr)". Never display those placeholders as real translations. */
static inline int rf_text_translation_available(const char *p,uint32_t bytes)
{
    while(bytes && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n')){++p;--bytes;}
    if(!bytes)return 0;
    if(bytes>=3 && p[0]=='(' && (p[1]=='f' || p[1]=='F' || p[1]=='g' || p[1]=='G') &&
       (p[2]=='r' || p[2]=='R'))return 0;
    return 1;
}
#endif
