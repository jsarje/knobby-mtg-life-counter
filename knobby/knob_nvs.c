#include "knob_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"

// ---------- cached state ----------
static bool settings_dirty = false;
static int cached_brightness = DEFAULT_BRIGHTNESS_PERCENT;
static bool cached_auto_dim = false;

// ---------- init ----------
void knob_nvs_init(void)
{
    nvs_flash_init();

    nvs_handle_t handle;
    if (nvs_open("knobby", NVS_READONLY, &handle) == ESP_OK) {
        int8_t dim_val = 0;
        int8_t bri_val = DEFAULT_BRIGHTNESS_PERCENT;
        nvs_get_i8(handle, "auto_dim", &dim_val);
        nvs_get_i8(handle, "brightness", &bri_val);
        cached_auto_dim = (dim_val != 0);
        cached_brightness = clamp_brightness(bri_val);
        nvs_close(handle);
    }
}

// ---------- getters ----------
int nvs_get_brightness(void)
{
    return cached_brightness;
}

bool nvs_get_auto_dim(void)
{
    return cached_auto_dim;
}

// ---------- setters ----------
void nvs_set_brightness(int value)
{
    cached_brightness = clamp_brightness(value);
    settings_dirty = true;
}

void nvs_set_auto_dim(bool value)
{
    cached_auto_dim = value;
    settings_dirty = true;
}

// ---------- persist ----------
void settings_save(void)
{
    if (!settings_dirty) return;
    nvs_handle_t handle;
    if (nvs_open("knobby", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i8(handle, "auto_dim", cached_auto_dim ? 1 : 0);
        nvs_set_i8(handle, "brightness", (int8_t)cached_brightness);
        nvs_commit(handle);
        nvs_close(handle);
        settings_dirty = false;
    }
}
