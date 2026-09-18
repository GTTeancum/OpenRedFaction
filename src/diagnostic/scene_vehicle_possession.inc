/* One stationary vehicle/driver first-pass ownership. RE contracts427c1e and
 * 4a1970 require collision admission before exit mutation and restore ordinary
 * player control afterward. Host dynamics and camera/seat transforms remain
 * the typed host backend's responsibility, never a proxy player physics body.
 * Borrowed control records deliberately contain no health/armor/inventory.
 * Handles include registry generation; callbacks must resolve them anew.
 * All callbacks are read-only and must leave game state unchanged on failure.
 * Exit callback must validate complete player-body path/occupancy/ground, not
 * merely return a guessed offset or the original preboarding position. */
#include "rf/entity.h"
#include <math.h>
#include <string.h>
typedef struct scene_vehicle_pose {float position[3],basis[9];} scene_vehicle_pose;
typedef struct scene_vehicle_player_control {
    uint32_t handle,host,control,movement,control_flags;
    int32_t weapon;
    scene_vehicle_pose pose;
} scene_vehicle_player_control;
typedef struct scene_vehicle_host_control {uint32_t handle,driver,alive;} scene_vehicle_host_control;
typedef struct scene_vehicle_possession {
    uint32_t active,host,driver;
    scene_vehicle_player_control saved;
} scene_vehicle_possession;
typedef struct scene_vehicle_possession_ops {
    int (*enter_allowed)(void *,uint32_t player,uint32_t host,uint32_t *allowed);
    int (*seat_pose)(void *,uint32_t host,scene_vehicle_pose *);
    int (*exit_pose)(void *,uint32_t player,uint32_t host,scene_vehicle_pose *,uint32_t *clear);
    void *context;
} scene_vehicle_possession_ops;
static int scene_vehicle_pose_valid(const scene_vehicle_pose *pose)
{
    uint32_t i;for(i=0;i<3;i++)if(!isfinite(pose->position[i]))return 0;
    for(i=0;i<9;i++)if(!isfinite(pose->basis[i]))return 0;
    return 1;
}
static int scene_vehicle_possess(scene_vehicle_possession *session,
    scene_vehicle_player_control *player,scene_vehicle_host_control *host,
    uint32_t player_alive,const scene_vehicle_possession_ops *ops,uint32_t *entered)
{
    scene_vehicle_pose seat;uint32_t allowed=0;int status;
    if(!session || !player || !host || !ops || !entered || !ops->enter_allowed || !ops->seat_pose)return RF_RANGE;
    *entered=0;
    if(session->active || !player_alive || !host->alive || host->driver!=UINT32_MAX ||
       player->host!=UINT32_MAX || player->handle==UINT32_MAX || host->handle==UINT32_MAX ||
       player->handle==host->handle)return RF_OK;
    status=ops->enter_allowed(ops->context,player->handle,host->handle,&allowed);if(status)return status;
    if(!allowed)return RF_OK;
    status=ops->seat_pose(ops->context,host->handle,&seat);if(status)return status;
    if(!scene_vehicle_pose_valid(&seat))return RF_FORMAT;
    session->saved=*player;session->host=host->handle;session->driver=player->handle;session->active=1;
    host->driver=player->handle;player->host=host->handle;player->control=host->handle;player->pose=seat;
    *entered=1;return RF_OK;
}
/* reason0 Use exit,1 cancel/unpossess,2 player death. Cancel for a living
 * player still requires safe placement. Host destruction is a normal safe exit
 * attempt; if blocked keep owner/resources alive until an exit is available
 * or player death resolves it. Never teleport a living driver through walls.
 * Death cleanup clears links at the current pose without inventing an exit. */
static int scene_vehicle_unpossess(scene_vehicle_possession *session,
    scene_vehicle_player_control *player,scene_vehicle_host_control *host,
    uint32_t reason,const scene_vehicle_possession_ops *ops,uint32_t *exited)
{
    scene_vehicle_pose destination;scene_vehicle_player_control restored;uint32_t clear=0;int status;
    if(!session || !player || !host || !exited || reason>2)return RF_RANGE;
    *exited=0;if(!session->active)return RF_OK;
    if(session->host!=host->handle || session->driver!=player->handle ||
       host->driver!=player->handle || player->host!=host->handle)return RF_NOT_FOUND;
    destination=player->pose;
    if(reason!=2){
        if(!ops || !ops->exit_pose)return RF_RANGE;
        status=ops->exit_pose(ops->context,player->handle,host->handle,&destination,&clear);if(status)return status;
        if(!clear)return RF_OK;
        if(!scene_vehicle_pose_valid(&destination))return RF_FORMAT;
    }
    restored=session->saved;restored.pose=destination;
    *player=restored;host->driver=UINT32_MAX;memset(session,0,sizeof(*session));
    *exited=1;return RF_OK;
}
