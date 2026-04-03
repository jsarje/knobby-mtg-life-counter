#ifndef _KNOB_NVS_H
#define _KNOB_NVS_H

#include "knob_types.h"

void knob_nvs_init(void);
void settings_save(void);

int nvs_get_brightness(void);
void nvs_set_brightness(int value);
bool nvs_get_auto_dim(void);
void nvs_set_auto_dim(bool value);

#endif // _KNOB_NVS_H
