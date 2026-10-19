#ifndef VAPORUI_WIDGET_UTIL_H
#define VAPORUI_WIDGET_UTIL_H

#include "../../include/vaporui.h"

// Columns a character takes in an editable field; a stray control character still takes a cell.
static inline unsigned int cell_w(VuiCodepoint c)
{
    int w = vui_cp_width(c);

    return w > 0 ? (unsigned int)w : 1;
}

static inline unsigned int cells(const VuiCodepoint *text, unsigned int from, unsigned int to)
{
    unsigned int c = 0;

    for (; from < to; from++) c += cell_w(text[from]);
    return c;
}

#endif
