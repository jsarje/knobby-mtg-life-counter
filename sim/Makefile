# Knobby Screenshot Simulator
# Requires: gcc, LVGL installed via arduino-cli (see CONTRIBUTING.md)

# Include configuration for paths
-include ../config.mk

# Default values if not passed from config.sh
LLVM_BIN  ?= 
LVGL_PATH ?= $(HOME)/Arduino/libraries/lvgl
PYTHON    ?= python3

SDL2_ROOT := SDL2/x86_64-w64-mingw32

KNOBBY    := ../knobby
BUILD     := build
BIN       := knobby_sim
BIN_GUI   := knobby_sim_gui.exe

CC        := $(LLVM_BIN)/clang.exe
SDL_CFLAGS := -I$(SDL2_ROOT)/include
SDL_LDFLAGS := -L$(SDL2_ROOT)/lib -lSDL2main -lSDL2

CFLAGS    := -Wall -Wno-unused-function \
             -DLV_CONF_INCLUDE_SIMPLE -DSIMULATOR=1 \
             -I. -Iesp_stubs -I$(LVGL_PATH) -I$(KNOBBY) \
             $(SDL_CFLAGS) \
             -O2 -g

LDFLAGS   := $(SDL_LDFLAGS) -lm

# ---- Source files ----

# Knobby UI/logic files (NOT: knobby.ino, board_detect.c, bidi_switch_knob.c)
KNOBBY_SRCS := $(addprefix $(KNOBBY)/, \
	knob.c knob_scr_main.c knob_scr_multiplayer.c knob_scr_menus.c \
	knob_intro.c knob_timer.c knob_life.c knob_damage_log.c \
	knob_game_mode.c knob_rename.c knob_dice.c knob_hw.c knob_nvs.c \
	mana_counter_icons_16.c mana_poison_icon_bold_16.c)

# Sim files
SIM_SRCS := sim_main.c sim_stubs.c

# LVGL sources (recursive wildcard)
rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
LVGL_SRCS := $(call rwildcard,$(LVGL_PATH)/src/,*.c)

FONT_SRCS := $(wildcard $(KNOBBY)/src/fonts/*.c)

ALL_SRCS := $(SIM_SRCS) $(KNOBBY_SRCS) $(FONT_SRCS) $(LVGL_SRCS)
ALL_SRCS_GUI := sim_sdl2.c sim_stubs.c $(KNOBBY_SRCS) $(FONT_SRCS) $(LVGL_SRCS)

# Object files — flatten into build/ using path-mangled names
obj_name = $(BUILD)/$(subst :,_COL_,$(subst /,__,$(subst ..,UP,$(1:.c=.o))))
ALL_OBJS := $(foreach src,$(ALL_SRCS),$(call obj_name,$(src)))
ALL_OBJS_GUI := $(foreach src,$(ALL_SRCS_GUI),$(call obj_name,$(src)))

# ---- Targets ----

.PHONY: screenshot generate-matrix clean gui web sim-gui

$(BIN): $(ALL_OBJS)
	$(CC) $(ALL_OBJS) -o $@ $(LDFLAGS)

sim-gui: $(BIN_GUI)
	./$(BIN_GUI)

gui: $(BIN_GUI)
	./$(BIN_GUI)

$(BIN_GUI): $(ALL_OBJS_GUI)
	$(CC) $(ALL_OBJS_GUI) -o $@ $(LDFLAGS)

web:
	$(PYTHON) gen_web_sources.py $(LVGL_PATH) $(KNOBBY)
	$(EMCC) $(CFLAGS) -o knobby_web.js @web_sources.txt \
		-s USE_SDL=2 -s WASM=1 \
		-s INITIAL_MEMORY=33554432 \
		-s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'

screenshot: $(BIN)
	@if not exist screenshots mkdir screenshots
	./$(BIN) $(ARGS)

generate-matrix: $(BIN)
	./generate_matrix.sh

clean:
	rm -rf $(BUILD) $(BIN) $(BIN_GUI)

# ---- Build rules ----

$(BUILD):
	@if not exist $(BUILD) mkdir $(BUILD)

define compile_rule
$(call obj_name,$(1)): $(1) | $(BUILD)
	$(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach src,$(sort $(ALL_SRCS) $(ALL_SRCS_GUI)),$(eval $(call compile_rule,$(src))))
