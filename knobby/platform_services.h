#pragma once

// Hardware service interfaces.  All functions are implemented in
// platform_services.cpp and called only from C++ translation units.

bool  knobby_platform_services_init_ui(void);
void  knobby_platform_apply_brightness_percent(int percent);
float knob_read_battery_voltage(void);
