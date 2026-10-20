// picker: a tiny fzf. Reads lines from stdin when piped (`find . | build/picker`), else lists the
// files under the current directory. Prints the chosen line to stdout.
//
// Verify:
//   - typing narrows the list live (fuzzy: letters in order, case-insensitive; best matches first)
//   - Up/Down/PageUp/PageDown move the cursor (also while the input has focus), the wheel scrolls,
//     clicking a row picks it; Enter prints the highlighted line and exits; Escape / Ctrl-C exit
//     without printing
//   - works with 100k+ lines (try `seq 1 200000 | build/picker`): no lag while typing
//   - the UI draws on the terminal even though stdin/stdout are pipes: `echo a b c | build/picker | cat`
//   - the count line shows matches / total

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/vaporui.h"

typedef struct { unsigned int index; int score; } Match;

static char **lines;
static unsigned int line_count, line_cap;
static Match *matches;
static unsigned int match_count;
static VuiInput *query;
static VuiList *list;
static char count_text[48];
static const char *chosen;

static void add_line(char *s)
{
    if (line_count == line_cap) {
        line_cap = line_cap ? line_cap * 2 : 1024;
        lines = realloc(lines, line_cap * sizeof(*lines));
    }

    lines[line_count++] = s;
}

static void walk(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    struct dirent *e;

    while (d != NULL && (e = readdir(d)) != NULL && line_count < 200000) {
        char path[1024];

        if (e->d_name[0] == '.') {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);

        if (e->d_type == DT_DIR && depth < 6) {
            walk(path, depth + 1);
        } else if (e->d_type != DT_DIR) {
            add_line(strdup(path[0] == '.' && path[1] == '/' ? path + 2 : path));
        }
    }

    if (d != NULL) closedir(d);
}

// Subsequence match; consecutive letters and word starts score higher. -1 = no match.
static int score(const char *line, const char *pat)
{
    int s = 0, run = 0;

    for (const char *p = line; *p != '\0' && *pat != '\0'; p++) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*pat)) {
            run = 0;
            continue;
        }

        s += 1 + run * 2 + (p == line || strchr("/_-. ", p[-1]) != NULL ? 3 : 0);
        run++;
        pat++;
    }

    return *pat == '\0' ? s * 100 - (int)strlen(line) : -1;
}

static int by_score(const void *a, const void *b)
{
    const Match *x = a, *y = b;

    return y->score != x->score ? y->score - x->score : (int)x->index - (int)y->index;
}

static void refilter(VuiNode *input)
{
    char pat[128];

    (void)input;
    input_get_text(query, pat, sizeof(pat));
    match_count = 0;

    for (unsigned int i = 0; i < line_count; i++) {
        int s = pat[0] == '\0' ? 0 : score(lines[i], pat);

        if (s >= 0) matches[match_count++] = (Match){ i, s };
    }

    if (pat[0] != '\0') qsort(matches, match_count, sizeof(Match), by_score);

    list_set_count(list, match_count);
    list_set_cursor(list, 0);
    snprintf(count_text, sizeof(count_text), "%u/%u", match_count, line_count);
}

static const char *item(VuiList *l, unsigned int i)
{
    (void)l;
    return lines[matches[i].index];
}

static void pick(VuiList *l, unsigned int i)
{
    (void)i;
    chosen = lines[matches[l->cursor].index];
    vui_quit(vui_ctx());
}

static void submit(VuiNode *input)
{
    (void)input;
    if (match_count > 0) pick(list, list->cursor);
}

// Navigation keys work from the input too, so the cursor never needs focus.
static int handler(VuiCtx *ui, const VuiEvent *e, void *userdata)
{
    (void)userdata;

    if (e->type == VUI_EVENT_KEY && ((e->key.key == VUI_KEY_CHAR && e->key.character == 'c' && e->key.modifiers == VUI_MOD_CTRL)
        || (e->key.key == VUI_KEY_ESCAPE && e->key.modifiers == 0))) {
        vui_quit(ui);
        return 1;
    }

    if (e->type == VUI_EVENT_KEY && e->key.modifiers == 0 && vui_focus(ui) == &query->node
        && (e->key.key == VUI_KEY_UP || e->key.key == VUI_KEY_DOWN || e->key.key == VUI_KEY_PAGE_UP || e->key.key == VUI_KEY_PAGE_DOWN)) {
        return list->node.on_event(&list->node, ui, e);
    }

    return 0;
}

int main(void)
{
    VuiCtx *ui;
    VuiNode *root;
    VuiLabel *count, *prompt;
    VuiNode *head;
    char buf[1024];

    if (!isatty(STDIN_FILENO)) {
        while (fgets(buf, sizeof(buf), stdin) != NULL && line_count < 1000000) {
            buf[strcspn(buf, "\n")] = '\0';
            add_line(strdup(buf));
        }
    } else {
        walk(".", 0);
    }

    matches = malloc((line_count + 1) * sizeof(Match));
    ui = vui_init();

    if (ui == NULL || matches == NULL) {
        return 1;
    }

    vui_set_mouse(ui, 2);

    root = node_create();
    root->direction = VUI_DIR_COLUMN;

    head = node_create();
    head->basis = 1;
    head->gap = 1;
    prompt = label_create(">");
    prompt->style = (VuiStyle){ .fg = VUI_COLOR_CYAN, .attr = VUI_ATTR_BOLD };
    query = input_create(127);
    query->node.grow = 1;
    query->on_change = refilter;
    query->on_submit = submit;
    node_add(head, &prompt->node);
    node_add(head, &query->node);
    node_add(root, head);

    count = label_create(count_text);
    count->style = (VuiStyle){ .attr = VUI_ATTR_BOLD };
    node_add(root, &count->node);

    list = list_create(0, item);
    list->on_select = pick;
    node_add(root, &list->node);

    refilter(NULL);
    vui_set_focus(ui, &query->node);
    vui_run(ui, root, handler, NULL);

    node_destroy(root);
    vui_shutdown(ui);

    if (chosen != NULL) {
        puts(chosen);
    }

    return chosen == NULL;
}
