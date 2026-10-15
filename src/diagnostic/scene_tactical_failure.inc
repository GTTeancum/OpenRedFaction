#ifndef RF_SCENE_TACTICAL_FAILURE_INCLUDED
#define RF_SCENE_TACTICAL_FAILURE_INCLUDED
/* First structural failure, retained for QMP without retrying a callback.
 * source: 1 service, 2 navigation, 3 publication, 4 facing. */
uint32_t rf_scene_tactical_failure[3];
static int scene_tactical_failure(int status,uint32_t source,uint32_t line)
{
    if(status && status!=RF_NOT_FOUND && !rf_scene_tactical_failure[0]){
        rf_scene_tactical_failure[0]=(uint32_t)status;
        rf_scene_tactical_failure[1]=source;rf_scene_tactical_failure[2]=line;
    }
    return status;
}
#endif
