#ifndef __MODULE_IPTV_H__
#define __MODULE_IPTV_H__

#include <SDL2/SDL.h>
#include "module_common.h"

ModuleExitReason IPTVModule_run(SDL_Surface* screen);

// Release the module's browse-state allocations (sorted channel index). Call
// once at app teardown, before IPTV_curated_cleanup().
void IPTVModule_cleanup(void);

#endif
