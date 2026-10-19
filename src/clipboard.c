#include <stdlib.h>
#include <string.h>

#include "../include/vaporui.h"
#include "ctx.h"

void (*vui_clipboard_hook)(const char *utf8);

static char *clip;

void vui_clipboard_set(const char *utf8)
{
    char *copy = strdup(utf8);

    if (copy == NULL) {
        return;
    }

    free(clip);
    clip = copy;

    if (vui_clipboard_hook != NULL) {
        vui_clipboard_hook(clip);
    }
}

const char *vui_clipboard_get(void)
{
    return clip != NULL ? clip : "";
}
