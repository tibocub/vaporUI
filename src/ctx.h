#ifndef VAPORUI_CTX_H
#define VAPORUI_CTX_H

#include "../include/vaporui.h"

struct VuiCtx {
    VuiSize size;
    VuiScreen *screen;
    VuiNode *focus;
    VuiNode *hover; // deepest node under the pointer; it and its ancestors have ->hovered set
};

#endif
