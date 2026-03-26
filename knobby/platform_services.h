#ifndef KNOBBY_PLATFORM_SERVICES_H
#define KNOBBY_PLATFORM_SERVICES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool knobby_platform_services_init_ui(void);
void knobby_platform_apply_brightness_percent(int percent);

#ifdef __cplusplus
}
#endif

#endif
