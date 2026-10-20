#include <stdio.h>
#include <stdlib.h>

#include "../include/vaporui.h"
#include "utf8.h"

struct VuiScreen {
    unsigned int width;
    unsigned int height;
    VuiCell *front; // what's actually on the terminal right now
    VuiCell *back;  // what the app has drawn, not yet flushed
    VuiRect clip;
};

// Second half of a double-width character; never written to the terminal.
#define CONT_CELL 0xFFFFFFFEu

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
    scr->clip = (VuiRect){ 0, 0, width, height };

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
    scr->clip = (VuiRect){ 0, 0, width, height };
}

unsigned int scr_width(VuiScreen *scr)
{
    return scr->width;
}

unsigned int scr_height(VuiScreen *scr)
{
    return scr->height;
}

static int in_clip(const VuiScreen *scr, unsigned int x, unsigned int y)
{
    VuiRect c = scr->clip;

    return x < scr->width && y < scr->height && (int)x >= c.x && (int)y >= c.y
        && (int)x - c.x < (int)c.width && (int)y - c.y < (int)c.height;
}

// Overwriting half of a wide character must not leave the other half behind.
static void unglue(VuiScreen *scr, unsigned int x, unsigned int y)
{
    VuiCell *row = &scr->back[y * scr->width];

    if (row[x].codepoint == CONT_CELL && x > 0) {
        row[x - 1].codepoint = ' ';
    } else if (x + 1 < scr->width && row[x + 1].codepoint == CONT_CELL) {
        row[x + 1].codepoint = ' ';
    }
}

void scr_set(VuiScreen *scr, unsigned int x, unsigned int y, VuiCodepoint cp, VuiStyle style)
{
    int w = vui_cp_width(cp);

    if (w == 0 || !in_clip(scr, x, y)) {
        return;
    }

    if (w == 2 && !in_clip(scr, x + 1, y)) {
        cp = ' '; // half a wide character can't be shown
        w = 1;
    }

    VuiCell *row = &scr->back[y * scr->width];

    unglue(scr, x, y);
    row[x] = (VuiCell){ .codepoint = cp, .style = style };

    if (w == 2) {
        unglue(scr, x + 1, y);
        row[x + 1] = (VuiCell){ .codepoint = CONT_CELL, .style = style };
    }
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
            scr_set(scr, x, y, cp, style);
            x += vui_cp_width(cp);
        }
    }
}

void scr_fill(VuiScreen *scr, VuiRect rect, VuiCodepoint cp, VuiStyle style)
{
    for (unsigned int y = 0; y < rect.height; y++) {
        for (unsigned int x = 0; x < rect.width; x++) {
            scr_set(scr, rect.x + x, rect.y + y, cp, style);
        }
    }
}

VuiRect scr_clip(VuiScreen *scr, VuiRect clip)
{
    VuiRect old = scr->clip;

    scr->clip = clip;
    return old;
}

void scr_clear(VuiScreen *scr, VuiStyle style)
{
    fill(scr->back, scr->width * scr->height, (VuiCell){ .codepoint = ' ', .style = style });
}

static int color_depth = 24;

void vui_set_color_depth(int depth)
{
    color_depth = depth;
}

// SGR parameter for one color; base is 30 (foreground) or 40 (background).
// Colors beyond the terminal's depth are approximated.
static int color_sgr(char *buf, int size, VuiColor c, int base)
{
    unsigned int r = 0, g = 0, b = 0;

    if (c <= 16) {
        return c <= 8 ? snprintf(buf, size, ";%d", base + (int)c - 1) : snprintf(buf, size, ";%d", base + 60 + (int)c - 9);
    }

    if (c & 0x02000000u) {
        unsigned int n = c & 0xff;

        if (color_depth >= 8) {
            return snprintf(buf, size, ";%d;5;%u", base + 8, n);
        }

        // 16 colors: take a palette entry's nearest basic color from its cube coordinates
        if (n < 16) {
            return color_sgr(buf, size, n + 1, base);
        }

        r = n >= 232 ? (n - 232) * 10 : ((n - 16) / 36) * 51;
        g = n >= 232 ? r : (((n - 16) / 6) % 6) * 51;
        b = n >= 232 ? r : ((n - 16) % 6) * 51;
    } else {
        r = (c >> 16) & 0xff;
        g = (c >> 8) & 0xff;
        b = c & 0xff;
    }

    if (color_depth >= 24) {
        return snprintf(buf, size, ";%d;2;%u;%u;%u", base + 8, r, g, b);
    }

    if (color_depth >= 8) {
        if (r == g && g == b) {
            return snprintf(buf, size, ";%d;5;%u", base + 8, r < 8 ? 16 : r > 248 ? 231 : 232 + (r - 8) / 10);
        }

        return snprintf(buf, size, ";%d;5;%u", base + 8, 16 + 36 * ((r * 5 + 127) / 255) + 6 * ((g * 5 + 127) / 255) + (b * 5 + 127) / 255);
    }

    {
        unsigned int index = (r > 127) | (g > 127) << 1 | (b > 127) << 2, bright = r > 191 || g > 191 || b > 191;

        return color_sgr(buf, size, index + 1 + (bright && index ? 8 : 0), base);
    }
}

static int style_sgr(VuiStyle style, char *buf, int bufsize)
{
    int n = snprintf(buf, bufsize, "\033[0");

    if (style.attr & VUI_ATTR_BOLD)      n += snprintf(buf + n, bufsize - n, ";1");
    if (style.attr & VUI_ATTR_UNDERLINE) n += snprintf(buf + n, bufsize - n, ";4");
    if (style.attr & VUI_ATTR_REVERSE)   n += snprintf(buf + n, bufsize - n, ";7");
    if (style.fg != VUI_COLOR_DEFAULT)   n += color_sgr(buf + n, bufsize - n, style.fg, 30);
    if (style.bg != VUI_COLOR_DEFAULT)   n += color_sgr(buf + n, bufsize - n, style.bg, 40);

    n += snprintf(buf + n, bufsize - n, "m");

    return n;
}

void scr_flush(VuiScreen *scr)
{
    VuiStyle last_style = {0};
    int have_style = 0;
    int cur_x = -1;
    int cur_y = -1;

    for (unsigned int y = 0; y < scr->height; y++) {
        for (unsigned int x = 0; x < scr->width; x++) {
            VuiCell *back = &scr->back[y * scr->width + x];
            VuiCell *front = &scr->front[y * scr->width + x];

            if (back->codepoint == front->codepoint && style_eq(back->style, front->style)) {
                continue;
            }

            if (back->codepoint == CONT_CELL) {
                *front = *back; // the wide character before it already covered this cell
                continue;
            }

            // Only reposition the cursor if we're not already sitting
            // right after the previous written cell.
            if (cur_x != (int)x || cur_y != (int)y) {
                printf("\033[%u;%uH", y + 1, x + 1);
            }

            if (!have_style || !style_eq(back->style, last_style)) {
                char sgr[64];
                int n = style_sgr(back->style, sgr, sizeof(sgr));
                fwrite(sgr, 1, n, stdout);
                last_style = back->style;
                have_style = 1;
            }

            uint8_t utf8[4];
            int len = vui_utf8_encode(back->codepoint, utf8);
            fwrite(utf8, 1, len, stdout);

            *front = *back;
            cur_x = x + (back->codepoint != CONT_CELL && x + 1 < scr->width && back[1].codepoint == CONT_CELL ? 2 : 1);
            cur_y = y;
        }
    }

    fflush(stdout);
}
