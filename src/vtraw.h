#ifndef VAPORUI_VTRAW_H
#define VAPORUI_VTRAW_H

#include <stdint.h>
#include "../include/vaporui.h"

// The Linux console in K_MEDIUMRAW: the kernel stops translating and reports which physical key went down or up.
// This file is pure (no ioctl, no linux headers): the backend reads the kernel keymap with KDGKBENT and the accent
// table with KDGKBDIACRUC into a VuiVtRaw, then feeds it every byte read from the console. See docs/input.md.

#define VUI_VT_KEYS    256 // keycodes
#define VUI_VT_TABLES  4   // keymaps used: none, Shift, AltGr, Shift+AltGr (the kernel indexes them by BIT(modifier))
#define VUI_VT_DIACR   256

typedef enum { VT_NONE, VT_CHAR, VT_LETTER, VT_FN, VT_CUR, VT_PAD, VT_SPEC, VT_SHIFT, VT_DEAD } VuiVtType;

typedef struct { uint8_t type; uint32_t value; } VuiVtEntry; // value: a codepoint (CHAR, LETTER, DEAD) or the kernel's KVAL

typedef struct { uint32_t diacr, base, result; } VuiVtAccent; // struct kbdiacruc

typedef struct {
    VuiVtEntry map[VUI_VT_TABLES][VUI_VT_KEYS];
    VuiVtAccent accent[VUI_VT_DIACR];
    unsigned int accents;

    uint8_t down[VUI_VT_KEYS];     // keys held
    uint8_t caps, num;             // lock state (the app owns it in raw mode)
    uint8_t compose;               // Compose pressed: the next character acts as a dead key
    uint32_t dead;                 // accent waiting for its letter, 0 none
    int pending;                   // bytes of a 3-byte keycode already read (0, 1, 2)
    int pending_down;
    unsigned int pending_code;
    VuiEvent queue[3];             // a key can produce more than one event (an accent that doesn't combine)
    unsigned int queue_len;
    int switch_to;                 // set to N when Ctrl+Alt+F<N> was pressed: the backend must VT_ACTIVATE it
} VuiVtRaw;

// One keymap value from KDGKBENT (kb_value) -> entry. The kernel returns Unicode characters as value ^ 0xF000.
VuiVtEntry vui_vtraw_entry(unsigned int kb_value);

void vui_vtraw_init(VuiVtRaw *raw, int numlock); // keymap and accents are filled by the caller; this resets the rest

// One byte from the console. Returns the next event or VUI_EVENT_NONE; more may follow: call vui_vtraw_pop until NONE.
VuiEvent vui_vtraw_feed(VuiVtRaw *raw, uint8_t byte);
VuiEvent vui_vtraw_pop(VuiVtRaw *raw);

// Forget held keys (after a VT switch the release events go to another console)
void vui_vtraw_release_all(VuiVtRaw *raw);

#endif
