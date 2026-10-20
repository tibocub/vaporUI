#include "../include/vaporui.h"

#define DEFAULTS { \
    .focus     = { .attr = VUI_ATTR_REVERSE | VUI_ATTR_BOLD }, \
    .hover     = { .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLUE }, \
    .selection = { .attr = VUI_ATTR_REVERSE }, \
    .title     = { .fg = VUI_COLOR_BLACK, .bg = VUI_COLOR_WHITE, .attr = VUI_ATTR_BOLD }, \
    .title_off = { .fg = VUI_COLOR_BLACK, .bg = VUI_COLOR_BRIGHT_BLACK }, \
    .popup     = { .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLACK }, \
    .bar       = { .attr = VUI_ATTR_REVERSE }, \
    .thumb     = { .attr = VUI_ATTR_REVERSE }, \
    .scroll    = { .fg = VUI_COLOR_BRIGHT_BLACK }, \
}

static VuiTheme theme = DEFAULTS;

VuiTheme *vui_theme(void)
{
    return &theme;
}

// Statics survive between runs on systems that load apps once (NuttX builtins): put the defaults back.
void vui_theme_reset(void)
{
    theme = (VuiTheme)DEFAULTS;
}
