#include "../include/vaporui.h"

static VuiTheme theme = {
    .focus     = { .attr = VUI_ATTR_REVERSE | VUI_ATTR_BOLD },
    .hover     = { .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLUE },
    .selection = { .attr = VUI_ATTR_REVERSE },
    .title     = { .fg = VUI_COLOR_BLACK, .bg = VUI_COLOR_WHITE, .attr = VUI_ATTR_BOLD },
    .title_off = { .fg = VUI_COLOR_BLACK, .bg = VUI_COLOR_BRIGHT_BLACK },
    .popup     = { .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLACK },
    .bar       = { .attr = VUI_ATTR_REVERSE },
    .thumb     = { .attr = VUI_ATTR_REVERSE },
    .scroll    = { .fg = VUI_COLOR_BRIGHT_BLACK },
};

VuiTheme *vui_theme(void)
{
    return &theme;
}
