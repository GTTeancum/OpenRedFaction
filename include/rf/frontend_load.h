#ifndef RF_FRONTEND_LOAD_H
#define RF_FRONTEND_LOAD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Sources have independent generation counters. The frontend chooses one
 * explicitly; neither this API nor menu display searches for a latest save. */
enum { RF_SCENE_FRONTEND_LOAD_MANUAL=0,RF_SCENE_FRONTEND_LOAD_AUTO=1 };
enum {
    RF_SCENE_FRONTEND_LOAD_READY=0,
    RF_SCENE_FRONTEND_LOAD_MISSING,
    RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE,
    RF_SCENE_FRONTEND_LOAD_ERROR,
    RF_SCENE_FRONTEND_LOAD_BLOCKED
};
/* Explicit idle-frontend action only, after successful session retirement.
 * READY arms the existing native cold loader and returns its level name;
 * it is preflight success, not proof that the complete world has restored.
 * Missing/incompatible/error keep the frontend idle after successful close.
 * BLOCKED requires restart: an owner failed retirement or storage close.
 * Caller must not reset the session between READY and cold construction.
 * Does not write saves, synthesize a stream, or publish a level transition. */
int rf_scene_frontend_load_prepare(uint32_t source,char level[64],uint32_t *outcome);
#ifdef __cplusplus
}
#endif
#endif
