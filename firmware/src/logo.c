/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the splash screen (build 46; Tonitor's layout): the mark -- the three stacks, their fumes, their red
 * reflection -- large in the middle; the lettering across the width under it; the build at the foot. The website's
 * logo (tools/make_logo.py) rendered at the screen's size (tools/make_splash.py, assets/splash/), 16 colours an image
 * (tools/gen_splash.py: norilsk_splash.h) */
#include "norilsk_splash.h"

#define SPLASH_MARK_Y 6u
#define SPLASH_WORD_Y 150u
#define SPLASH_BUILD_Y 208u

/* an image of the header across rows y .. y + h of the screen, centred, over black */
static void splash_img(uint32_t y, uint32_t w, uint32_t h, const uint16_t *pal, const uint8_t *px)
{
    uint32_t x0 = (240u - w) / 2u, stride = (w + 1u) / 2u;
    CV_AREA(0, y, 240, h, C_BLACK) {
        uint32_t i, j;
        for (j = 0; j < cv_h; j++) {
            const uint8_t *row = px + ((uint32_t)cv_oy + j) * stride;
            for (i = 0; i < w; i++)
                cv_px[j * cv_w + x0 + i] = pal[(row[i >> 1] >> ((i & 1u) ? 0 : 4)) & 15u];
        }
    }
}
static void splash_draw(const char *build)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    splash_img(SPLASH_MARK_Y, SPLASH_MARK_W, SPLASH_MARK_H, SPLASH_MARK_PAL, SPLASH_MARK_PX);
    splash_img(SPLASH_WORD_Y, SPLASH_WORD_W, SPLASH_WORD_H, SPLASH_WORD_PAL, SPLASH_WORD_PX);
    draw_text_box(0, SPLASH_BUILD_Y, 240, &FONT_S, build, C_GREY, 1);
}
typedef char splash_fits[SPLASH_MARK_Y + SPLASH_MARK_H <= SPLASH_WORD_Y && SPLASH_WORD_Y + SPLASH_WORD_H <= SPLASH_BUILD_Y &&
                         SPLASH_MARK_W <= 240u && SPLASH_WORD_W <= 240u ? 1 : -1];
