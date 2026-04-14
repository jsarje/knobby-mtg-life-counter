/*******************************************************************************
 * Size: 18 px
 * Bpp: 4
 * Opts: --font .tmp\mana-font\mana.ttf -r 0xE618 --size 18 --bpp 4 --format lvgl --lv-include lvgl.h -o knobby\mana_poison_icon_bold_16.c --lv-font-name mana_poison_icon_bold_16 --no-compress
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef MANA_POISON_ICON_BOLD_16
#define MANA_POISON_ICON_BOLD_16 1
#endif

#if MANA_POISON_ICON_BOLD_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+E618 "" */
    0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x70, 0x0, 0x0, 0x0, 0x0, 0x3, 0x90,
    0x0, 0x0, 0x0, 0x0, 0x6, 0xa0, 0x0, 0x0,
    0x0, 0x0, 0x4d, 0xc2, 0x0, 0x0, 0x0, 0x1b,
    0xee, 0xff, 0xc2, 0x0, 0x0, 0xdc, 0x8, 0xc1,
    0xad, 0x10, 0x6, 0xe0, 0x6, 0xd0, 0xa, 0x80,
    0xd, 0x70, 0x5, 0xf0, 0x3, 0xe0, 0xf, 0x50,
    0x7, 0xe0, 0x2, 0xf0, 0xf, 0x60, 0x7, 0xc0,
    0x2, 0xf0, 0xa, 0xc0, 0x6, 0xc0, 0x7, 0xc0,
    0x1, 0xeb, 0x5, 0xe0, 0x7f, 0x40, 0x0, 0x2d,
    0xdc, 0xfd, 0xe4, 0x0, 0x0, 0x0, 0x5c, 0xe6,
    0x10, 0x0, 0x0, 0x0, 0x5, 0xb0, 0x0, 0x0,
    0x0, 0x0, 0x2, 0xc0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x70, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 288, .box_w = 12, .box_h = 19, .ofs_x = 3, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 58904, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t mana_poison_icon_bold_16 = {
#else
lv_font_t mana_poison_icon_bold_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 19,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if MANA_POISON_ICON_BOLD_16*/

