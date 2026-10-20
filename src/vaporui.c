#include <stdlib.h>

#include "ctx.h"


int vui_platform_init(void);
void vui_platform_shutdown(void);

int vui_platform_get_size(VuiSize *size);
void vui_platform_clear(void);
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready);
uint64_t vui_platform_now_ms(void);
void vui_platform_set_mouse(int level);

static VuiCtx *the_ui;

VuiCtx *vui_ctx(void)
{
    return the_ui;
}

VuiCtx *vui_init(void)
{
    VuiCtx *ui;

    if (vui_platform_init() != 0) {
        return NULL;
    }

    ui = calloc(1, sizeof(*ui));

    if (ui == NULL) {
        vui_platform_shutdown();
        return NULL;
    }

    if (vui_platform_get_size(&ui->size) != 0) {
        free(ui);
        vui_platform_shutdown();
        return NULL;
    }

    ui->screen = scr_create(ui->size.width, ui->size.height);

    if (ui->screen == NULL) {
        free(ui);
        vui_platform_shutdown();
        return NULL;
    }

    vui_platform_clear();
    the_ui = ui;

    return ui;
}

void vui_shutdown(VuiCtx *ui)
{
    if (ui == NULL) {
        return;
    }

    for (unsigned int i = 0; i < ui->overlay_count; i++) {
        node_destroy(ui->overlays[i].node);
    }

    vui_overlays_free(ui);
    scr_destroy(ui->screen);
    vui_platform_shutdown();
    the_ui = NULL;
    free(ui);
}

unsigned int vui_width(VuiCtx *ui)
{
    return ui->size.width;
}

unsigned int vui_height(VuiCtx *ui)
{
    return ui->size.height;
}

// Clears the back buffer -- nothing reaches the terminal until the
// next vui_flush().
void vui_clear(VuiCtx *ui)
{
    scr_clear(ui->screen, (VuiStyle){0});
}

void vui_flush(VuiCtx *ui)
{
    scr_flush(ui->screen);
}

VuiScreen *vui_screen(VuiCtx *ui)
{
    return ui->screen;
}

int vui_watch_fd(VuiCtx *ui, int fd, VuiFdCallback cb, void *userdata)
{
    for (unsigned int i = 0; i < ui->watch_count; i++) {
        if (ui->watches[i].fd == fd) {
            ui->watches[i] = (VuiWatch){ fd, cb, userdata };
            return 0;
        }
    }

    if (ui->watch_count == VUI_MAX_WATCH) {
        return -1;
    }

    ui->watches[ui->watch_count++] = (VuiWatch){ fd, cb, userdata };
    return 0;
}

void vui_unwatch_fd(VuiCtx *ui, int fd)
{
    for (unsigned int i = 0; i < ui->watch_count; i++) {
        if (ui->watches[i].fd == fd) {
            ui->watches[i] = ui->watches[--ui->watch_count];
            return;
        }
    }
}

int vui_timer(VuiCtx *ui, unsigned int ms, int repeat, VuiTimerCallback cb, void *userdata)
{
    for (int i = 0; i < VUI_MAX_TIMERS; i++) {
        if (ui->timers[i].id == 0) {
            ui->timers[i] = (VuiTimer){ ++ui->last_timer_id, vui_platform_now_ms() + ms, ms, repeat, cb, userdata };
            return ui->timers[i].id;
        }
    }

    return 0;
}

void vui_timer_cancel(VuiCtx *ui, int id)
{
    for (int i = 0; i < VUI_MAX_TIMERS; i++) {
        if (id != 0 && ui->timers[i].id == id) {
            ui->timers[i].id = 0;
        }
    }
}

// ms until the earliest timer, or -1 if there is none
static int timer_timeout(VuiCtx *ui)
{
    uint64_t now = vui_platform_now_ms(), soonest = 0;
    int found = 0;

    for (int i = 0; i < VUI_MAX_TIMERS; i++) {
        if (ui->timers[i].id != 0 && (!found || ui->timers[i].due < soonest)) {
            soonest = ui->timers[i].due;
            found = 1;
        }
    }

    return !found ? -1 : soonest <= now ? 0 : (int)(soonest - now);
}

static int run_timers(VuiCtx *ui)
{
    uint64_t now = vui_platform_now_ms();
    int fired = 0;

    for (int i = 0; i < VUI_MAX_TIMERS; i++) {
        VuiTimer t = ui->timers[i];

        if (t.id == 0 || t.due > now) {
            continue;
        }

        if (t.repeat) {
            ui->timers[i].due = now + (t.interval ? t.interval : 1);
        } else {
            ui->timers[i].id = 0;
        }

        t.cb(ui, t.userdata);
        fired = 1;
    }

    return fired;
}

VuiEvent vui_poll_event(VuiCtx *ui)
{
    int fds[VUI_MAX_WATCH];
    unsigned char ready[VUI_MAX_WATCH];
    unsigned int n = ui->watch_count;
    int fired;

    for (unsigned int i = 0; i < n; i++) {
        fds[i] = ui->watches[i].fd;
    }

    VuiEvent event = vui_platform_poll_event(timer_timeout(ui), fds, n, ready);

    if (event.type == VUI_EVENT_RESIZE) {
        ui->size = event.size;
        scr_resize(ui->screen, event.size.width, event.size.height);
    }

    fired = run_timers(ui);

    for (unsigned int i = 0; i < n; i++) {
        if (!ready[i]) {
            continue;
        }

        // the callback of an earlier fd may have removed this one
        for (unsigned int j = 0; j < ui->watch_count; j++) {
            if (ui->watches[j].fd == fds[i]) {
                VuiWatch w = ui->watches[j];

                w.cb(ui, w.fd, w.userdata);
                fired = 1;
                break;
            }
        }
    }

    if (event.type == VUI_EVENT_NONE && fired) {
        event.type = VUI_EVENT_WAKE;
    }

    return event;
}

void vui_set_mouse(VuiCtx *ui, int level)
{
    (void)ui;
    vui_platform_set_mouse(level);
}
