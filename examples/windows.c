// windows: floating windows (overlays), modal dialog, drop-down, bars, scrolling, timers, themes.
//
// Verify:
//   - the top bar shows menu labels and a clock that ticks every second (a vui_timer)
//   - "New window" opens a window (up to 6): drag its title bar to move it, its bottom-right
//     corner to resize it, [x] to close it; clicking a window raises it; only the top window's
//     title bar is highlighted; a window can't be dragged completely off screen
//   - each window holds a text area; typing goes to the window on top
//   - "Scroll" opens a window with 12 input fields in a scroll container: the wheel, the
//     scrollbar (click / drag) and Tab all scroll; Tab to a hidden field scrolls it into view
//   - "Quit" opens a modal dialog: Tab stays inside it, clicking outside does nothing, Escape
//     or "No" closes it (focus returns to the Quit button), "Yes" quits
//   - the drop-down switches theme (classic / dos / truecolor): Enter, Space, Down or a click
//     opens a popup list; Escape or a click outside closes it without changing anything
//   - the bottom bar names the theme; Ctrl-C quits

#include <stdio.h>
#include <time.h>

#include "../include/vaporui.h"

#define MAX_WINDOWS 6

static const char *themes[] = { "classic", "dos", "truecolor" };
static VuiTheme classic;
static char titles[MAX_WINDOWS][24];
static int opened;
static char clock_text[16];
static char theme_text[48] = "theme: classic";

static void tick(VuiCtx *ui, void *userdata)
{
    time_t now = time(NULL);

    (void)ui;
    (void)userdata;
    strftime(clock_text, sizeof(clock_text), "%H:%M:%S", localtime(&now));
}

static void set_theme(VuiNode *dropdown, unsigned int i)
{
    VuiTheme *t = vui_theme();

    (void)dropdown;
    *t = classic;

    if (i == 1) { // blue desktop, cyan bars
        VuiStyle blue = { VUI_COLOR_WHITE, VUI_COLOR_BLUE, 0 };

        t->text = t->border = t->scroll = blue;
        t->popup = (VuiStyle){ VUI_COLOR_BLACK, VUI_COLOR_CYAN, 0 };
        t->title = t->bar = t->thumb = (VuiStyle){ VUI_COLOR_BLACK, VUI_COLOR_CYAN, 0 };
        t->title_off = (VuiStyle){ VUI_COLOR_BLACK, VUI_COLOR_WHITE, 0 };
        t->hover = (VuiStyle){ VUI_COLOR_BLACK, VUI_COLOR_YELLOW, 0 };
    } else if (i == 2) { // 24-bit colors, approximated on terminals with less
        t->text = (VuiStyle){ VUI_RGB(220, 220, 230), VUI_RGB(24, 26, 38), 0 };
        t->border = (VuiStyle){ VUI_RGB(120, 140, 220), VUI_RGB(24, 26, 38), 0 };
        t->scroll = t->border;
        t->popup = (VuiStyle){ VUI_RGB(220, 220, 230), VUI_RGB(44, 48, 70), 0 };
        t->title = t->thumb = (VuiStyle){ VUI_RGB(20, 20, 30), VUI_RGB(130, 160, 255), VUI_ATTR_BOLD };
        t->title_off = (VuiStyle){ VUI_RGB(20, 20, 30), VUI_RGB(90, 96, 130), 0 };
        t->bar = (VuiStyle){ VUI_RGB(230, 230, 240), VUI_RGB(60, 64, 96), 0 };
        t->hover = (VuiStyle){ VUI_RGB(20, 20, 30), VUI_RGB(255, 200, 100), 0 };
    }

    snprintf(theme_text, sizeof(theme_text), "theme: %s", themes[i]);
}

static void new_window(VuiNode *button)
{
    VuiWindow *w;
    VuiTextarea *ta;

    (void)button;

    if (opened >= MAX_WINDOWS) {
        return;
    }

    snprintf(titles[opened], sizeof(titles[0]), "Window %d", opened + 1);
    w = window_create(titles[opened]);
    ta = textarea_create(0);
    ta->node.grow = 1;
    textarea_set_text(ta, "Drag the title bar. Resize from the corner. Type here.");
    node_add(&w->node, &ta->node);
    vui_overlay_add(vui_ctx(), &w->node, (VuiRect){ 22 + opened * 4, 3 + opened * 2, 36, 9 }, 0);
    opened++;
}

static void scroll_window(VuiNode *button)
{
    VuiWindow *w = window_create("Scroll");
    VuiNode *scroll = scroll_create();

    (void)button;
    w->node.direction = VUI_DIR_COLUMN;
    scroll->gap = 1;

    for (int i = 0; i < 12; i++) {
        VuiInput *in = input_create(30);
        static char labels[12][16];

        snprintf(labels[i], sizeof(labels[i]), "field %d", i + 1);
        input_set_text(in, labels[i]);
        in->node.basis = 1;
        node_add(scroll, &in->node);
    }

    node_add(&w->node, scroll);
    vui_overlay_add(vui_ctx(), &w->node, (VuiRect){ 30, 5, 32, 10 }, 0);
}

static void close_dialog(VuiNode *button)
{
    vui_overlay_remove(vui_ctx(), button);
}

static void confirm_quit(VuiNode *button)
{
    (void)button;
    vui_quit(vui_ctx());
}

static void quit_dialog(VuiNode *button)
{
    VuiBorder *frame = border_create("Quit?");
    VuiLabel *msg = label_create("Really quit?");
    VuiNode *row = node_create();
    VuiButton *yes = button_create("Yes", confirm_quit), *no = button_create("No", close_dialog);

    (void)button;
    frame->node.direction = VUI_DIR_COLUMN;
    row->gap = 2;
    row->justify = VUI_JUSTIFY_CENTER;
    row->basis = 1;
    node_add(row, &yes->node);
    node_add(row, &no->node);
    node_add(&frame->node, &msg->node);
    node_add(&frame->node, row);
    vui_overlay_add(vui_ctx(), &frame->node, (VuiRect){ 25, 8, 24, 5 }, VUI_OVERLAY_MODAL | VUI_OVERLAY_DISMISS);
}

static int handler(VuiCtx *ui, const VuiEvent *e, void *userdata)
{
    (void)userdata;

    if (e->type == VUI_EVENT_KEY && e->key.key == VUI_KEY_CHAR && e->key.character == 'c' && e->key.modifiers == VUI_MOD_CTRL) {
        vui_quit(ui);
        return 1;
    }

    return 0;
}

static VuiButton *add_button(VuiNode *parent, const char *label, void (*on_click)(VuiNode *))
{
    VuiButton *b = button_create(label, on_click);

    node_add(parent, &b->node);
    return b;
}

static void draw_desktop(VuiNode *node, VuiScreen *scr, void *userdata)
{
    (void)userdata;
    scr_fill(scr, node->rect, 0x2591, vui_theme()->scroll);
}

static VuiLabel *bar_label(VuiDiv *bar, const char *text)
{
    VuiLabel *l = label_create(text);

    l->style = vui_theme()->bar;
    node_add(&bar->node, &l->node);
    return l;
}

int main(void)
{
    VuiCtx *ui = vui_init();
    VuiNode *root, *desktop, *spacer;
    VuiDiv *top, *bottom;
    VuiDropdown *theme;

    if (ui == NULL) {
        return 1;
    }

    classic = *vui_theme();
    vui_set_mouse(ui, 2);
    tick(ui, NULL);
    vui_timer(ui, 1000, 1, tick, NULL);

    root = node_create();
    root->direction = VUI_DIR_COLUMN;

    top = bar_create();
    bar_label(top, "vaporUI");
    bar_label(top, " File  Edit  View");
    spacer = node_create();
    spacer->grow = 1;
    node_add(&top->node, spacer);
    bar_label(top, clock_text);
    node_add(root, &top->node);

    desktop = node_create();
    desktop->grow = 1;
    desktop->direction = VUI_DIR_COLUMN;
    desktop->gap = 1;
    node_pad(desktop, 1, 2, 1, 2);
    desktop->draw = draw_desktop;
    desktop->align = VUI_ALIGN_START;
    node_add(root, desktop);

    add_button(desktop, "New window", new_window);
    add_button(desktop, "Scroll", scroll_window);
    theme = dropdown_create(themes, 3, set_theme);
    node_add(desktop, &theme->node);
    add_button(desktop, "Quit", quit_dialog);

    bottom = bar_create();
    bar_label(bottom, theme_text);
    node_add(root, &bottom->node);

    vui_set_focus(ui, desktop->children[0]);
    vui_run(ui, root, handler, NULL);

    node_destroy(root);
    vui_shutdown(ui);

    return 0;
}
