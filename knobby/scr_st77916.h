#ifndef _SCR_ST77916_H_
#define _SCR_ST77916_H_

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_RES_HOR 360
#define SCREEN_RES_VER 360

bool scr_lvgl_init(void);
void setRotation(uint8_t rot);
void screen_switch(bool on);
void set_brightness(uint8_t bri);

#endif
