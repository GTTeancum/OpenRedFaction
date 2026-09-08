#include "rf/timer.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    struct { rf_game_clock clock; int32_t deadline,operation,value; } input;
    struct { int32_t status; rf_game_clock clock; int32_t deadline,result; } output;
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(&input,sizeof(input),1,stdin)==1) {
        output.clock=input.clock; output.deadline=input.deadline; output.result=123;
        switch (input.operation) {
        case 0: output.status=rf_clock_advance(&output.clock,input.value); break;
        case 1: output.status=rf_clock_pause(&output.clock); break;
        case 2: output.status=rf_clock_resume(&output.clock); break;
        case 3: output.status=rf_timer_set(&output.deadline,input.clock.game_ms,input.value); break;
        case 4: output.status=rf_timer_expired(input.deadline,input.clock.game_ms,&output.result); break;
        case 5: output.status=rf_timer_remaining(input.deadline,input.clock.game_ms,&output.result); break;
        case 6: rf_timer_clear(&output.deadline); output.status=RF_OK; break;
        default: return 2;
        }
        if (fwrite(&output,sizeof(output),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
