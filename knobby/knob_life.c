#include "knob_life.h"

// Forward declarations for UI refresh (defined in screen modules)
extern void refresh_main_ui(void);
extern void refresh_select_ui(void);
extern void refresh_damage_ui(void);
extern void refresh_multiplayer_ui(void);
extern void refresh_multiplayer_cmd_select_ui(void);
extern void refresh_multiplayer_cmd_damage_ui(void);
extern void refresh_multiplayer_all_damage_ui(void);

// ---------- state ----------
int life_total = DEFAULT_LIFE_TOTAL;
int pending_life_delta = 0;
bool life_preview_active = false;

enemy_state_t enemies[ENEMY_COUNT] = {
    {"P1", 0},
    {"P2", 0},
    {"P3", 0}
};

int selected_enemy = -1;
int dice_result = 0;

int multiplayer_life[MULTIPLAYER_COUNT] = {40, 40, 40, 40};
int multiplayer_selected = 0;
char multiplayer_names[MULTIPLAYER_COUNT][16] = {"P1", "P2", "P3", "P4"};
int multiplayer_menu_player = 0;
int multiplayer_cmd_source = 0;
int multiplayer_cmd_target = -1;
int multiplayer_cmd_delta = 0;
int multiplayer_cmd_damage_totals[MULTIPLAYER_COUNT][MULTIPLAYER_COUNT] = {{0}};
int multiplayer_all_damage_value = 0;
int multiplayer_pending_life_delta = 0;
int multiplayer_preview_player = -1;
bool multiplayer_life_preview_active = false;
int multiplayer_cmd_target_choices[MULTIPLAYER_COUNT - 1] = {-1, -1, -1};

static lv_timer_t *life_preview_timer = NULL;
static lv_timer_t *multiplayer_life_preview_timer = NULL;

// ---------- player colors ----------
lv_color_t get_player_base_color(int index)
{
    static const uint32_t colors[MULTIPLAYER_COUNT] = {0x7B1FE0, 0x29B6F6, 0xFFD600, 0xA5D6A7};
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x303030);
    return lv_color_hex(colors[index]);
}

lv_color_t get_player_active_color(int index)
{
    static const uint32_t colors[MULTIPLAYER_COUNT] = {0x9C4DFF, 0x4FC3F7, 0xFFEA61, 0xC8E6C9};
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x505050);
    return lv_color_hex(colors[index]);
}

lv_color_t get_player_text_color(int index)
{
    return (index == 2) ? lv_color_black() : lv_color_white();
}

lv_color_t get_player_preview_color(int index, int delta)
{
    if (index == 2) {
        return (delta < 0) ? lv_color_hex(0x7A1020) : lv_color_hex(0x215A2A);
    }
    return (delta < 0) ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN);
}

int get_main_player_index(void)
{
    int i;

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (strcmp(multiplayer_names[i], "m") == 0) {
            return i;
        }
    }

    return -1;
}

int get_cmd_target_player_index(int row)
{
    int main_player = get_main_player_index();
    int count = 0;
    int i;

    if (row < 0 || row >= ENEMY_COUNT) return row;

    if (main_player < 0) {
        return row;
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (i == main_player) continue;
        if (count == row) return i;
        count++;
    }

    return row;
}

// ---------- life preview ----------
static void life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;

    life_total = clamp_life(life_total + pending_life_delta);
    pending_life_delta = 0;
    life_preview_active = false;
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    if (life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    }
    refresh_main_ui();
    refresh_select_ui();
}

void multiplayer_life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!multiplayer_life_preview_active ||
        multiplayer_preview_player < 0 ||
        multiplayer_preview_player >= MULTIPLAYER_COUNT) {
        if (multiplayer_life_preview_timer != NULL) {
            lv_timer_pause(multiplayer_life_preview_timer);
        }
        return;
    }

    multiplayer_life[multiplayer_preview_player] = clamp_life(
        multiplayer_life[multiplayer_preview_player] + multiplayer_pending_life_delta
    );
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
    refresh_multiplayer_ui();
}

// ---------- life changes ----------
void change_life(int delta)
{
    pending_life_delta += delta;
    pending_life_delta = clamp_life(life_total + pending_life_delta) - life_total;
    life_preview_active = (pending_life_delta != 0);

    if (life_preview_timer != NULL) {
        lv_timer_reset(life_preview_timer);
    }

    if (!life_preview_active && life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    } else if (life_preview_timer != NULL) {
        lv_timer_resume(life_preview_timer);
    }

    refresh_main_ui();
}

void add_damage_to_selected_enemy(int delta)
{
    if (selected_enemy < 0 || selected_enemy >= ENEMY_COUNT) return;

    enemies[selected_enemy].damage += delta;
    if (enemies[selected_enemy].damage < 0)
        enemies[selected_enemy].damage = 0;

    if (delta > 0) {
        change_life(-delta);
    } else if (delta < 0) {
        change_life(-delta);
    }

    refresh_damage_ui();
    refresh_select_ui();
}

void change_multiplayer_life(int delta)
{
    int preview_base;

    if (multiplayer_selected < 0 || multiplayer_selected >= MULTIPLAYER_COUNT) return;

    if (multiplayer_life_preview_active &&
        multiplayer_preview_player != multiplayer_selected) {
        multiplayer_life_preview_commit_cb(NULL);
    }

    multiplayer_preview_player = multiplayer_selected;
    preview_base = multiplayer_life[multiplayer_preview_player];
    multiplayer_pending_life_delta += delta;
    multiplayer_pending_life_delta = clamp_life(preview_base + multiplayer_pending_life_delta) - preview_base;
    multiplayer_life_preview_active = (multiplayer_pending_life_delta != 0);

    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_reset(multiplayer_life_preview_timer);
    }

    if (!multiplayer_life_preview_active && multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
        multiplayer_preview_player = -1;
    } else if (multiplayer_life_preview_timer != NULL) {
        lv_timer_resume(multiplayer_life_preview_timer);
    }

    refresh_multiplayer_ui();
}

void change_multiplayer_cmd_damage(int delta)
{
    int updated_total;

    if (multiplayer_cmd_target < 0 || multiplayer_cmd_target >= MULTIPLAYER_COUNT) return;
    if (multiplayer_cmd_source < 0 || multiplayer_cmd_source >= MULTIPLAYER_COUNT) return;

    updated_total = multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target] + delta;
    if (updated_total < 0) {
        delta = -multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target];
        updated_total = 0;
    }

    multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target] = updated_total;
    multiplayer_cmd_delta = updated_total;

    multiplayer_life[multiplayer_cmd_target] = clamp_life(multiplayer_life[multiplayer_cmd_target] - delta);
    refresh_multiplayer_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
}

void change_multiplayer_all_damage(int delta)
{
    multiplayer_all_damage_value += delta;
    if (multiplayer_all_damage_value < 0) multiplayer_all_damage_value = 0;
    refresh_multiplayer_all_damage_ui();
}

// ---------- reset ----------
void knob_life_reset(void)
{
    int i;

    life_total = DEFAULT_LIFE_TOTAL;
    pending_life_delta = 0;
    life_preview_active = false;
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    selected_enemy = -1;
    dice_result = 0;

    for (i = 0; i < ENEMY_COUNT; i++) {
        enemies[i].damage = 0;
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_life[i] = DEFAULT_LIFE_TOTAL;
        snprintf(multiplayer_names[i], sizeof(multiplayer_names[i]), "P%d", i + 1);
    }
    multiplayer_selected = 0;
    multiplayer_menu_player = 0;
    multiplayer_cmd_source = 0;
    multiplayer_cmd_target = -1;
    multiplayer_cmd_delta = 0;
    memset(multiplayer_cmd_damage_totals, 0, sizeof(multiplayer_cmd_damage_totals));
    multiplayer_all_damage_value = 0;

    if (life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    }
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
}

// ---------- init ----------
void knob_life_init(void)
{
    life_preview_timer = lv_timer_create(life_preview_commit_cb, 4000, NULL);
    if (life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    }
    multiplayer_life_preview_timer = lv_timer_create(multiplayer_life_preview_commit_cb, 4000, NULL);
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
}
