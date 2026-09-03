#include <stdio.h>
#include <stdlib.h>

#include "../include/vaporui.h"
#include "utf8.h"

struct VuiScreen {
    unsigned int width;
    unsigned int height;
    VuiCell *front; // what's actually on the terminal right now
    VuiCell *back;  // what the app has drawn, not yet flushed
};

static VuiCell blank_cell(void)
{
    return (VuiCell){ .codepoint = ' ', .style = {0} };
}

// No real codepoint is this value -- guarantees every cell reads as
// "changed" on the flush right after create/resize, forcing a full
// redraw instead of needing a separate first-frame code path.
static VuiCell sentinel_cell(void)
{
    return (VuiCell){ .codepoint = 0xFFFFFFFF, .style = {0} };
}

static int style_eq(VuiStyle a, VuiStyle b)
{
    return a.fg == b.fg && a.bg == b.bg && a.attr == b.attr;
}

static void fill(VuiCell *cells, unsigned int n, VuiCell value)
{
    for (unsigned int i = 0; i < n; i++) {
        cells[i] = value;
    }
}

VuiScreen *scr_create(unsigned int width, unsigned int height)
{
    VuiScreen *scr = malloc(sizeof(*scr));

    if (scr == NULL) {
        return NULL;
    }

    unsigned int n = width * height;

    scr->width = width;
    scr->height = height;
    scr->front = malloc(n * sizeof(VuiCell));
    scr->back = malloc(n * sizeof(VuiCell));

    if (scr->front == NULL || scr->back == NULL) {
        free(scr->front);
        free(scr->back);
        free(scr);
        return NULL;
    }

    fill(scr->front, n, sentinel_cell());
    fill(scr->back, n, blank_cell());

    return scr;
}

void scr_destroy(VuiScreen *scr)
{
    if (scr == NULL) {
        return;
    }

    free(scr->front);
    free(scr->back);
    free(scr);
}

void scr_resize(VuiScreen *scr, unsigned int width, unsigned int height)
{
    unsigned int n = width * height;
    VuiCell *front = realloc(scr->front, n * sizeof(VuiCell));
    VuiCell *back = realloc(scr->back, n * sizeof(VuiCell));

    if (front == NULL || back == NULL) {
        // Leave scr as it was rather than commit to a half-updated
        // size; caller keeps drawing at the old dimensions.
        return;
    }

    scr->front = front;
    scr->back = back;
    scr->width = width;
    scr->height = height;

    // Old content doesn't map onto the new dimensions in any
    // meaningful way -- force a full redraw instead of reflowing it.
    fill(scr->front, n, sentinel_cell());
    fill(scr->back, n, blank_cell());
}

unsigned int scr_width(VuiScreen *scr)
{
    return scr->width;
}

unsigned int scr_height(VuiScreen *scr)
{
    return scr->height;
}

void scr_set(VuiScreen *scr, unsigned int x, unsigned int y, VuiCodepoint cp, VuiStyle style)
{
    if (x >= scr->width || y >= scr->height) {
        return;
    }

    scr->back[y * scr->width + x] = (VuiCell){ .codepoint = cp, .style = style };
}

void scr_puts(VuiScreen *scr, unsigned int x, unsigned int y, const char *str, VuiStyle style)
{
    VuiUtf8Decoder decoder;

    vui_utf8_reset(&decoder);

    // No wide-character handling yet -- each codepoint advances one
    // cell, so CJK/emoji will misalign until that's added.
    while (*str != '\0') {
        uint32_t cp;

        if (vui_utf8_feed(&decoder, (uint8_t)*str++, &cp) == VUI_UTF8_COMPLETE) {
            scr_set(scr, x++, y, cp, style);
        }
    }
}

void scr_clear(VuiScreen *scr, VuiStyle style)
{
    fill(scr->back, scr->width * scr->height, (VuiCell){ .codepoint = ' ', .style = style });
}

static int style_sgr(VuiStyle style, char *buf, int bufsize)
{
    int n = snprintf(buf, bufsize, "\033[0");

    if (style.attr & VUI_ATTR_BOLD)      n += snprintf(buf + n, bufsize - n, ";1");
    if (style.attr & VUI_ATTR_UNDERLINE) n += snprintf(buf + n, bufsize - n, ";4");
    if (style.attr & VUI_ATTR_REVERSE)   n += snprintf(buf + n, bufsize - n, ";7");
    if (style.fg != VUI_COLOR_DEFAULT)   n += snprintf(buf + n, bufsize - n, ";%d", 30 + (style.fg - 1));
    if (style.bg != VUI_COLOR_DEFAULT)   n += snprintf(buf + n, bufsize - n, ";%d", 40 + (style.bg - 1));

    n += snprintf(buf + n, bufsize - n, "m");

    return n;
}

void scr_flush(VuiScreen *scr)
{
    VuiStyle last_style = {0};
    int have_style = 0;
    int cur_x = -1;
    int cur_y = -1;

    fputs("\033[?25l", stdout); // hide cursor -- avoid it jumping around mid-redraw

    for (unsigned int y = 0; y < scr->height; y++) {
        for (unsigned int x = 0; x < scr->width; x++) {
            VuiCell *back = &scr->back[y * scr->width + x];
            VuiCell *front = &scr->front[y * scr->width + x];

            if (back->codepoint == front->codepoint && style_eq(back->style, front->style)) {
                continue;
            }

            // Only reposition the cursor if we're not already sitting
            // right after the previous written cell.
            if (cur_x != (int)x || cur_y != (int)y) {
                printf("\033[%u;%uH", y + 1, x + 1);
            }

            if (!have_style || !style_eq(back->style, last_style)) {
                char sgr[32];
                int n = style_sgr(back->style, sgr, sizeof(sgr));
                fwrite(sgr, 1, n, stdout);
                last_style = back->style;
                have_style = 1;
            }

            uint8_t utf8[4];
            int len = vui_utf8_encode(back->codepoint, utf8);
            fwrite(utf8, 1, len, stdout);

            *front = *back;
            cur_x = x + 1;
            cur_y = y;
        }
    }

    fputs("\033[?25h", stdout);
    fflush(stdout);
}
