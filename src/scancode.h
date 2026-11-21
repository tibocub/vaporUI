#ifndef VAPORUI_SCANCODE_H
#define VAPORUI_SCANCODE_H

#include "../include/vaporui.h"

// Physical key tables, one per platform that reports raw keys (more come with their backends: Linux keycodes,
// macOS virtual key codes, the web's KeyboardEvent.code). vui_scancode_base() / _from_base() are in vaporui.h.

VuiScancode vui_scancode_from_set1(unsigned int sc, int extended);

#endif
