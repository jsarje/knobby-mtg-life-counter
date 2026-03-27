#pragma once

// Encoder input and GUI entry points.  All callers are C++ translation units.

#include "lvgl.h"
#include "bidi_switch_knob.h"

void knob_gui(void);
void knob_change(knob_event_t k, int cont);
void knob_process_pending(void);
