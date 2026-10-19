#include <stdlib.h>

#include "../include/vaporui.h"

typedef struct {
    VuiKey key;
    VuiModifiers modifiers;
    VuiCodepoint character;
    VuiAction action;
    void *userdata;
} Binding;

struct VuiControls {
    Binding *bindings;
    unsigned int count;
    unsigned int cap;
};

VuiControls *ctl_create(void)
{
    return calloc(1, sizeof(VuiControls));
}

void ctl_destroy(VuiControls *c)
{
    if (c == NULL) {
        return;
    }

    free(c->bindings);
    free(c);
}

static int same_key(const Binding *b, VuiKey key, VuiModifiers modifiers, VuiCodepoint character)
{
    if (b->key != key || b->modifiers != modifiers) {
        return 0;
    }

    return key != VUI_KEY_CHAR || b->character == character;
}

void ctl_bind(
    VuiControls *c,
    VuiKey key, VuiModifiers modifiers, VuiCodepoint character,
    VuiAction action, void *userdata
) {
    for (unsigned int i = 0; i < c->count; i++) {
        if (same_key(&c->bindings[i], key, modifiers, character)) {
            c->bindings[i].action = action;
            c->bindings[i].userdata = userdata;
            return;
        }
    }

    if (c->count == c->cap) {
        unsigned int cap = c->cap ? c->cap * 2 : 4;
        Binding *bindings = realloc(c->bindings, cap * sizeof(Binding));

        if (bindings == NULL) {
            return;
        }

        c->bindings = bindings;
        c->cap = cap;
    }

    c->bindings[c->count++] = (Binding){
        .key = key,
        .modifiers = modifiers,
        .character = character,
        .action = action,
        .userdata = userdata,
    };
}

int ctl_dispatch(VuiControls *c, VuiCtx *ui, const VuiKeyEvent *event)
{
    for (unsigned int i = 0; i < c->count; i++) {
        if (same_key(&c->bindings[i], event->key, event->modifiers, event->character)) {
            c->bindings[i].action(ui, event, c->bindings[i].userdata);
            return 1;
        }
    }

    return 0;
}
