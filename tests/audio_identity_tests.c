#include "rf/audio.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("failed line %d\n",__LINE__);return 1;}} while(0)
int main(void)
{
    rf_audio_mixer mixer;rf_audio_voice_ids ids,before;uint32_t source,out;int32_t id,old;
    const uint8_t pcm[2]={0,0};rf_wave_pcm wave={pcm,2,1,48000,1,16};int16_t output[4];
    rf_audio_mixer_init(&mixer);rf_audio_voice_ids_init(&ids);
    /* Cross the actual mixer sign boundary, without a long-duration test. */
    mixer.generation=32767;
    CHECK(!rf_audio_voice_start(&mixer,&wave,32768,32768,0,&source));
    CHECK(source&0x80000000u);
    CHECK(!rf_audio_voice_ids_bind(&ids,source,&id) && id==0);
    CHECK(!rf_audio_voice_ids_resolve(&ids,&mixer,id,&out) && out==source);
    rf_audio_mix(&mixer,output,2);
    CHECK(!mixer.voices[source&0xffff].active);
    CHECK(!rf_audio_voice_ids_resolve(&ids,&mixer,id,&out) && out==source);
    old=id;CHECK(!rf_audio_voice_stop(&mixer,source));out=123;
    CHECK(rf_audio_voice_ids_resolve(&ids,&mixer,old,&out)==RF_NOT_FOUND && out==123);
    CHECK(!rf_audio_voice_start(&mixer,&wave,32768,32768,1,&source));
    CHECK(rf_audio_voice_ids_resolve(&ids,&mixer,old,&out)==RF_NOT_FOUND && out==123);
    CHECK(!rf_audio_voice_ids_bind(&ids,source,&id) && id==1);
    CHECK(rf_audio_voice_ids_resolve(&ids,&mixer,old,&out)==RF_NOT_FOUND);
    /* Original nonnegative device sequence has its own wrap boundary. */
    ids.next=0x7fffffff;
    CHECK(!rf_audio_voice_ids_bind(&ids,source,&id) && id==INT32_MAX && ids.next==0);
    CHECK(!rf_audio_voice_ids_bind(&ids,source,&id) && id==0 && ids.next==1);
    before=ids;id=99;
    CHECK(rf_audio_voice_ids_bind(&ids,0,&id)==RF_RANGE && id==99 && !memcmp(&ids,&before,sizeof(ids)));
    CHECK(rf_audio_voice_ids_bind(&ids,0x1001e,&id)==RF_RANGE && !memcmp(&ids,&before,sizeof(ids)));
    CHECK(rf_audio_voice_ids_resolve(&ids,&mixer,-1,&out)==RF_NOT_FOUND);
    rf_audio_voice_ids_init(&ids);CHECK(rf_audio_voice_ids_resolve(&ids,&mixer,0,&out)==RF_NOT_FOUND);
    puts("PASS signed device identity, unsigned mixer sign boundary, expiry/reuse and wrap");return 0;
}
