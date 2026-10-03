#pragma once
#include "obs.h"
#define OBS_DECLARE_MODULE()
#define MODULE_EXPORT
#define OBS_MODULE_USE_DEFAULT_LOCALE(name, locale)
extern "C" char *obs_module_config_path(const char *);

extern "C" const char *obs_module_text(const char *);
