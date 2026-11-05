// stream: a string flows left to right on every line, each line starting one frame after the one above.
//
// Verify:
//   - frame 0: only the first character, top-left corner. Frame 1: it moves right, the second
//     character appears at column 0, and line 2 draws its first character. And so on: a diagonal wave
//   - the animation fills the whole terminal and follows resizes
//   - Ctrl-C quits

#include "../include/vaporui.h"

#define TEXT " ,,..""~~!!++::vvccIIooww00XXPP$$##**RRBB@@  //  VAPOR  //"
#define TEXT_LEN (sizeof(TEXT) - 1)

static unsigned int frame;

static void tick(VuiCtx *ui, void *userdata)
{
    (void)ui;
    (void)userdata;
    frame++;
}

// Line y has written frame - y + 1 characters; character j sits at column frame - y - j.
static void draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    (void)userdata;

    for (unsigned int y = 0; y < node->rect.height && y <= frame; y++) {
        for (unsigned int j = 0; j <= frame - y; j++) {
            unsigned int x = frame - y - j;

            if (x < node->rect.width)
						{
                scr_set(scr, node->rect.x + x,
								node->rect.y + y,
								(unsigned char)TEXT[(TEXT_LEN - j % TEXT_LEN) % TEXT_LEN],
								(VuiStyle){ 0 });
            }
        }
    }
}

static int on_key(VuiCtx *ui, const VuiEvent *e, void *userdata)
{
    (void)userdata;

    if (e->type == VUI_EVENT_KEY
				&& e->key.key == VUI_KEY_CHAR
				&& e->key.character == 'c'
				&& e->key.modifiers == VUI_MOD_CTRL)
		{
        vui_quit(ui);
        return 1;
    }

    return 0;
}

int main(void)
{
    VuiCtx *ui = vui_init();
    VuiNode *root;

    if (ui == NULL) {
        return 1;
    }

    root = node_create();   // a root fills the whole terminal on its own
    root->draw = draw;

    vui_timer(ui, 20, 1, tick, NULL);   // every 160 ms: run tick, then vui_run redraws
    vui_run(ui, root, on_key, NULL);

    node_destroy(root);
    vui_shutdown(ui);

    return 0;
}
