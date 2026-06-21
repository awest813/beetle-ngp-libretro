#ifndef MDFN_SETTINGS_H
#define MDFN_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t setting_ngp_language;
extern uint32_t setting_ngp_frameskip;

bool MDFN_GetSettingB(const char *name);

#ifdef __cplusplus
}
#endif

#endif
