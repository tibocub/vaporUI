// event-loop: a timer and a watched file descriptor next to the keyboard.
//
// Verify:
//   - "ticks" climbs 5 times a second without any input (a repeating vui_timer)
//   - a child process prints a line every half second; each appears in the list (vui_watch_fd on
//     its pipe) and the list follows the newest line until you move the cursor away
//   - keys, mouse and resizing still work while it runs; a pressed key never delays the output
//   - "Stop child" kills it: the list then shows "child exited" (EOF on the pipe, unwatched)
//   - Ctrl-C quits (and kills the child)

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/vaporui.h"

#define MAX_LINES 500

static char lines[MAX_LINES][80];
static unsigned int count;
static char partial[80];
static unsigned int partial_len;
static unsigned int ticks;
static char ticks_text[32];
static pid_t child_pid;
static VuiList *list;

static void push(const char *s)
{
    int tail = count == 0 || list->cursor + 1 >= count;

    if (count == MAX_LINES) {
        memmove(lines[0], lines[1], sizeof(lines) - sizeof(lines[0]));
        count--;
    }

    snprintf(lines[count++], sizeof(lines[0]), "%s", s);
    list_set_count(list, count);

    if (tail) {
        list_set_cursor(list, count - 1);
    }
}

static void on_child(VuiCtx *ui, int fd, void *userdata)
{
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf));

    (void)userdata;

    if (n <= 0) {
        vui_unwatch_fd(ui, fd);
        push("child exited");
        return;
    }

    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == '\n' || partial_len == sizeof(partial) - 1) {
            partial[partial_len] = '\0';
            push(partial);
            partial_len = 0;
        } else {
            partial[partial_len++] = buf[i];
        }
    }
}

static void spawn(VuiCtx *ui)
{
    int p[2];

    if (pipe(p) != 0) {
        return;
    }

    child_pid = fork();

    if (child_pid == 0) {
        dup2(p[1], STDOUT_FILENO);
        close(p[0]);
        close(p[1]);
        execl("/bin/sh", "sh", "-c", "i=0; while :; do echo \"line $i\"; i=$((i+1)); sleep 0.5; done", (char *)NULL);
        _exit(127);
    }

    close(p[1]);
    vui_watch_fd(ui, p[0], on_child, NULL);
}

static void on_tick(VuiCtx *ui, void *userdata)
{
    (void)ui;
    (void)userdata;
    snprintf(ticks_text, sizeof(ticks_text), "ticks: %u", ++ticks);
}

static void stop_child(VuiNode *button)
{
    (void)button;
    kill(child_pid, SIGTERM);
}

static const char *item(VuiList *l, unsigned int i)
{
    (void)l;
    return lines[i];
}

static int on_key(VuiCtx *ui, const VuiEvent *e, void *userdata)
{
    (void)userdata;

    if (e->type == VUI_EVENT_KEY && e->key.key == VUI_KEY_CHAR && e->key.character == 'c' && e->key.modifiers == VUI_MOD_CTRL) {
        vui_quit(ui);
        return 1;
    }

    return 0;
}

int main(void)
{
    VuiCtx *ui = vui_init();
    VuiNode *root, *head;
    VuiLabel *label;
    VuiButton *stop;

    if (ui == NULL) {
        return 1;
    }

    vui_set_mouse(ui, 2);
    on_tick(ui, NULL);

    root = node_create();
    root->direction = VUI_DIR_COLUMN;

    head = node_create();
    head->basis = 1;
    head->gap = 2;
    label = label_create(ticks_text);
    label->style = (VuiStyle){ .attr = VUI_ATTR_BOLD };
    stop = button_create("Stop child", stop_child);
    node_add(head, &label->node);
    node_add(head, &stop->node);
    node_add(root, head);

    list = list_create(0, item);
    node_add(root, &list->node);

    spawn(ui);

    vui_timer(ui, 200, 1, on_tick, NULL);
    vui_set_focus(ui, &list->node);
    vui_run(ui, root, on_key, NULL);

    kill(child_pid, SIGTERM);
    waitpid(child_pid, NULL, 0);

    node_destroy(root);
    vui_shutdown(ui);

    return 0;
}
