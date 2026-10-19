#ifndef VAPORUI_CTX_H
#define VAPORUI_CTX_H

#include "../include/vaporui.h"

struct VuiCtx {
    VuiSize size;
    VuiScreen *screen;
    VuiNode *focus;
    VuiNode *capture; // gets mouse events until the button is released
    VuiNode *hover; // deepest node under the pointer; it and its ancestors have ->hovered set
};

// Set by vui_init() to a backend that forwards copies to the system clipboard (OSC 52).
extern void (*vui_clipboard_hook)(const char *utf8);

#endif
