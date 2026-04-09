#ifndef _KNOB_HW_H
#define _KNOB_HW_H

#include "knob_types.h"

// ---------- state ----------
extern int brightness_percent;
extern bool auto_dim_enabled;
extern bool dimmed;
extern float battery_voltage;
extern int battery_percent;

// ---------- tunable power / timing constants (shared) ----------
// Increase AUTO_DIM_TIMEOUT_MS to reduce how often the device dims.
#define AUTO_DIM_TIMEOUT_MS     30000   /* ms idle before display dims */
// Reducing AUTO_DIM_BRIGHTNESS below 5 further cuts backlight draw while dimmed.
#define AUTO_DIM_BRIGHTNESS     5       /* % brightness while dimmed */
// UNDIM_GRACE_MS suppresses input for this long after wake to avoid accidental presses.
#define UNDIM_GRACE_MS          150     /* ms */
// Default active CPU frequency; some displays may be sensitive to lower CPU clocks.
#define CPU_FREQ_ACTIVE         160     /* MHz – reduced for power; test for display stability */
// 80 MHz is used while dimmed; lower values (e.g. 40) may be safe but are untested here.
#define CPU_FREQ_IDLE           80      /* MHz – used during light sleep / dimmed state */
// Battery sample throttle: how often a fresh ADC measurement is allowed.
#define BATTERY_SAMPLE_INTERVAL_MS  60000   /* ms between passive battery measurements */
// Auto-dim check period: how often the inactivity timer fires.
#define AUTO_DIM_CHECK_PERIOD_MS    1000    /* ms */

// ---------- functions ----------
void knob_hw_init(void);
void brightness_apply(void);
void update_battery_measurement(bool force);
int read_battery_percent(void);
void change_brightness(int delta);
bool in_undim_grace(void);

#endif // _KNOB_HW_H
