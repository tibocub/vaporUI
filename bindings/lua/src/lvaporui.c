/* vaporui: Lua binding for vaporUI. One module, Lua 5.1 to 5.4 and LuaJIT.
     vaporui.open([{mouse = 0|1|2}]) -> cols, rows | nil, err   raw terminal, alternate screen, hidden cursor, mouse reporting (vaporUI's platform layer)
     vaporui.close()                                            restore the terminal (also done when the Lua state closes)
     vaporui.poll([timeout_seconds]) -> name, ...               next input event, or nil, "timeout" (nil timeout: wait for one)
         "char", text   "key", name, mods, base, scancode   "mouse", action, button, x, y, mods   "resize", cols, rows   "paste", "start"|"end"   "quit"
         (coordinates are 1-based; mods: 1 shift, 2 alt, 4 ctrl, 8 meta)
     vaporui.size() -> cols, rows      vaporui.mouse(level)      vaporui.now() -> monotonic seconds
     vaporui.screen(cols, rows) -> a double-buffered cell grid: set, puts, fill, clear, flush, size, resize, close; vaporui.rgb/color256/width/depth/color
   poll() blocks the whole process while it waits, as it does in any single-threaded Lua program. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "vaporui.h"

/* the 5.2 to 5.4 helpers this file uses, for older Lua */
#if LUA_VERSION_NUM < 504
#define lua_newuserdatauv(L, sz, n) lua_newuserdata(L, sz)
#endif
#if LUA_VERSION_NUM < 502 && !defined(LUA_LJDIR) /* plain Lua 5.1 (LuaJIT has these) */
#define luaL_setmetatable(L, n) (luaL_getmetatable(L, n), lua_setmetatable(L, -2))
#define luaL_newlib(L, l) (lua_newtable(L), luaL_register(L, NULL, l))
static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
    (void)nup;
    luaL_register(L, NULL, l);
}
#endif

/* vaporUI's platform layer (src/platform/*.c); declared in src/vaporui.c too */
int vui_platform_init(void);
void vui_platform_shutdown(void);
int vui_platform_get_size(VuiSize *size);
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready);
void vui_platform_set_mouse(int level);
uint64_t vui_platform_now_ms(void);

/* normally defined by vaporui.c (the widget toolkit), which this binding does not link */
int vui_soft_pointer;

#define SCREEN_MT "vaporui.screen"
/* a codepoint as a UTF-8 string (lua_pushfstring's %U needs Lua 5.3) */
static void push_utf8(lua_State *L, unsigned long cp)
{
    char b[4];
    int n;

    if (cp < 0x80) { b[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else { b[0] = (char)(0xF0 | (cp >> 18)); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    lua_pushlstring(L, b, (size_t)n);
}

static const char *const keynames[] = {"", "", "enter", "backspace", "tab", "escape", "delete", "up", "down", "left", "right",
                                       "home", "end", "pageup", "pagedown", "insert"};

/* pushes the values of the event's tuple on the stack; returns how many (0: nothing to report for this event) */
static int push_values(lua_State *L, const VuiEvent *e)
{
    switch (e->type) {
    case VUI_EVENT_TEXT:
        lua_pushliteral(L, "char");
        push_utf8(L, e->text.codepoint);
        return 2;
    case VUI_EVENT_KEY: {
        const VuiKeyEvent *k = &e->key;

        lua_pushliteral(L, "key");
        if (k->key == VUI_KEY_CHAR) push_utf8(L, k->character);
        else if (k->key >= VUI_KEY_F1) lua_pushfstring(L, "f%d", (int)(k->key - VUI_KEY_F1 + 1));
        else lua_pushstring(L, keynames[k->key]);
        lua_pushinteger(L, (int)k->modifiers);
        /* the key's US-layout character and its physical key (USB HID number, 0 if unknown); "" rather than nil so tuples stay dense */
        if (k->key == VUI_KEY_CHAR && k->base) push_utf8(L, k->base);
        else lua_pushliteral(L, "");
        lua_pushinteger(L, (int)k->scancode);
        return 5;
    }
    case VUI_EVENT_MOUSE: {
        const VuiMouseEvent *m = &e->mouse;
        const char *act;
        int button = 0;

        switch (m->button) {
        case VUI_MOUSE_LEFT: button = 1; break;
        case VUI_MOUSE_MIDDLE: button = 2; break;
        case VUI_MOUSE_RIGHT: button = 3; break;
        default: break;
        }
        if (m->button == VUI_MOUSE_WHEEL_UP) act = "scroll_up";
        else if (m->button == VUI_MOUSE_WHEEL_DOWN) act = "scroll_down";
        else if (m->button == VUI_MOUSE_WHEEL_LEFT || m->button == VUI_MOUSE_WHEEL_RIGHT) return 0;
        else if (m->action == VUI_MOUSE_PRESS) act = "press";
        else if (m->action == VUI_MOUSE_RELEASE) act = "release";
        else act = button ? "drag" : "move";
        lua_pushliteral(L, "mouse");
        lua_pushstring(L, act);
        lua_pushinteger(L, button);
        lua_pushinteger(L, (lua_Integer)m->x + 1); /* 1-based */
        lua_pushinteger(L, (lua_Integer)m->y + 1);
        lua_pushinteger(L, (int)m->modifiers);
        return 6;
    }
    case VUI_EVENT_PASTE_START:
    case VUI_EVENT_PASTE_END:
        lua_pushliteral(L, "paste");
        lua_pushstring(L, e->type == VUI_EVENT_PASTE_START ? "start" : "end");
        return 2;
    case VUI_EVENT_RESIZE:
        lua_pushliteral(L, "resize");
        lua_pushinteger(L, e->size.width);
        lua_pushinteger(L, e->size.height);
        return 3;
    case VUI_EVENT_CLOSE:
        lua_pushliteral(L, "quit");
        return 1;
    default:
        return 0;
    }
}

typedef struct { VuiScreen *scr; } screen_t;

static VuiScreen *check_screen(lua_State *L, int i)
{
    screen_t *s = luaL_checkudata(L, i, SCREEN_MT);

    if (s->scr == NULL) luaL_error(L, "screen is closed");
    return s->scr;
}

/* style: nil (terminal default), or {fg=, bg=, bold=, underline=, reverse=}; colors are 0 (default), 1..16 (ANSI), ui.rgb(...) or ui.color256(...) */
static VuiStyle get_style(lua_State *L, int i)
{
    VuiStyle st = {0, 0, VUI_ATTR_NONE};

    if (lua_isnoneornil(L, i)) return st;
    luaL_checktype(L, i, LUA_TTABLE);
    lua_getfield(L, i, "fg"); if (!lua_isnil(L, -1)) st.fg = (VuiColor)luaL_checkinteger(L, -1); lua_pop(L, 1);
    lua_getfield(L, i, "bg"); if (!lua_isnil(L, -1)) st.bg = (VuiColor)luaL_checkinteger(L, -1); lua_pop(L, 1);
    lua_getfield(L, i, "bold"); if (lua_toboolean(L, -1)) st.attr |= VUI_ATTR_BOLD; lua_pop(L, 1);
    lua_getfield(L, i, "underline"); if (lua_toboolean(L, -1)) st.attr |= VUI_ATTR_UNDERLINE; lua_pop(L, 1);
    lua_getfield(L, i, "reverse"); if (lua_toboolean(L, -1)) st.attr |= VUI_ATTR_REVERSE; lua_pop(L, 1);
    return st;
}

/* a character argument: a codepoint number, or a string whose first UTF-8 character is used */
static VuiCodepoint get_char(lua_State *L, int i)
{
    if (lua_type(L, i) == LUA_TNUMBER) return (VuiCodepoint)luaL_checkinteger(L, i);
    {
        size_t n;
        const unsigned char *s = (const unsigned char *)luaL_checklstring(L, i, &n);
        unsigned cp;
        int extra;

        if (n == 0) return ' ';
        if (s[0] < 0x80) return s[0];
        if (s[0] >= 0xF0) { cp = s[0] & 7; extra = 3; }
        else if (s[0] >= 0xE0) { cp = s[0] & 15; extra = 2; }
        else if (s[0] >= 0xC0) { cp = s[0] & 31; extra = 1; }
        else return 0xFFFD;
        for (int k = 1; k <= extra; k++) {
            if ((size_t)k >= n || (s[k] & 0xC0) != 0x80) return 0xFFFD;
            cp = (cp << 6) | (s[k] & 0x3F);
        }
        return cp;
    }
}

static unsigned int coord(lua_State *L, int i) { return (unsigned int)(luaL_checkinteger(L, i) - 1); } /* 1-based; 0 wraps to a huge value that clips away */

static int l_screen(lua_State *L)
{
    unsigned int w = (unsigned int)luaL_optinteger(L, 1, 0), h = (unsigned int)luaL_optinteger(L, 2, 0);
    screen_t *s;

    if (w == 0 || h == 0) {
        int cols = 80, rows = 24;
        VuiSize sz;

        if (vui_platform_get_size(&sz) == 0 && sz.width > 0 && sz.height > 0) { cols = (int)sz.width; rows = (int)sz.height; }
        if (w == 0) w = (unsigned int)cols;
        if (h == 0) h = (unsigned int)rows;
    }
    if (w > 1000 || h > 1000) return luaL_error(L, "screen too large");
    s = lua_newuserdatauv(L, sizeof *s, 0);
    s->scr = scr_create(w, h);
    if (s->scr == NULL) return luaL_error(L, "out of memory");
    luaL_setmetatable(L, SCREEN_MT);
    return 1;
}

static int s_set(lua_State *L)
{
    scr_set(check_screen(L, 1), coord(L, 2), coord(L, 3), get_char(L, 4), get_style(L, 5));
    return 0;
}

static int s_puts(lua_State *L)
{
    size_t n;
    const char *t = luaL_checklstring(L, 4, &n);

    scr_putn(check_screen(L, 1), coord(L, 2), coord(L, 3), t, n, get_style(L, 5));
    return 0;
}

static int s_fill(lua_State *L)
{
    VuiRect r;

    r.x = (int)luaL_checkinteger(L, 2) - 1;
    r.y = (int)luaL_checkinteger(L, 3) - 1;
    r.width = (unsigned int)luaL_checkinteger(L, 4);
    r.height = (unsigned int)luaL_checkinteger(L, 5);
    scr_fill(check_screen(L, 1), r, get_char(L, 6), get_style(L, 7));
    return 0;
}

static int s_clear(lua_State *L) { scr_clear(check_screen(L, 1), get_style(L, 2)); return 0; }
static int s_flush(lua_State *L) { scr_flush(check_screen(L, 1)); return 0; }

static int s_size(lua_State *L)
{
    VuiScreen *s = check_screen(L, 1);

    lua_pushinteger(L, scr_width(s));
    lua_pushinteger(L, scr_height(s));
    return 2;
}

static int s_resize(lua_State *L)
{
    unsigned int w = (unsigned int)luaL_checkinteger(L, 2), h = (unsigned int)luaL_checkinteger(L, 3);

    if (w == 0 || h == 0 || w > 1000 || h > 1000) return luaL_error(L, "bad screen size");
    scr_resize(check_screen(L, 1), w, h);
    return 0;
}

static int s_close(lua_State *L)
{
    screen_t *s = luaL_checkudata(L, 1, SCREEN_MT);

    if (s->scr != NULL) { scr_destroy(s->scr); s->scr = NULL; }
    return 0;
}

static int l_rgb(lua_State *L)
{
    lua_pushinteger(L, VUI_RGB(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2), luaL_checkinteger(L, 3)));
    return 1;
}

static int l_color256(lua_State *L) { lua_pushinteger(L, VUI_COLOR256(luaL_checkinteger(L, 1))); return 1; }

static int l_width(lua_State *L) { lua_pushinteger(L, vui_str_width(luaL_checkstring(L, 1))); return 1; }

static int l_depth(lua_State *L) { vui_set_color_depth((int)luaL_checkinteger(L, 1)); return 0; }


/* ---- terminal: raw input, size, mouse ---- */
static int g_open;

static int l_open(lua_State *L)
{
    VuiSize sz;
    int mouse = 0;

    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "mouse");
        if (!lua_isnil(L, -1)) mouse = (int)luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }
    if (!g_open) {
        if (vui_platform_init() != 0) { lua_pushnil(L); lua_pushliteral(L, "no terminal"); return 2; }
        g_open = 1;
    }
    vui_platform_set_mouse(mouse);
    if (vui_platform_get_size(&sz) != 0) { sz.width = 80; sz.height = 24; }
    lua_pushinteger(L, sz.width);
    lua_pushinteger(L, sz.height);
    return 2;
}

static void do_close(void)
{
    if (g_open) { vui_platform_shutdown(); g_open = 0; }
}

static int l_close(lua_State *L) { (void)L; do_close(); return 0; }

static int l_size(lua_State *L)
{
    VuiSize sz;

    if (vui_platform_get_size(&sz) != 0) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, sz.width);
    lua_pushinteger(L, sz.height);
    return 2;
}

/* now() -> monotonic seconds (a float), for timers that match poll()'s timeout */
static int l_now(lua_State *L) { lua_pushnumber(L, (lua_Number)vui_platform_now_ms() / 1000.0); return 1; }

static int l_mouse(lua_State *L)
{
    if (g_open) vui_platform_set_mouse((int)luaL_checkinteger(L, 1));
    return 0;
}

/* poll([seconds]) -> name, ... | nil, "timeout" */
static int l_poll(lua_State *L)
{
    int forever = lua_isnoneornil(L, 1);
    double secs = forever ? 0 : luaL_checknumber(L, 1);
    unsigned long long deadline = vui_platform_now_ms() + (unsigned long long)(secs > 0 ? secs * 1000 : 0);
    unsigned char ready[1];

    if (!g_open) return luaL_error(L, "vaporui.open() first");
    for (;;) {
        long long left = 0;
        VuiEvent e;
        int n;

        if (!forever) {
            unsigned long long now = vui_platform_now_ms();
            left = now >= deadline ? 0 : (long long)(deadline - now);
        }
        e = vui_platform_poll_event(forever ? -1 : (int)left, NULL, 0, ready);
        n = push_values(L, &e);
        if (n > 0) return n;
        if (!forever && vui_platform_now_ms() >= deadline) { lua_pushnil(L); lua_pushliteral(L, "timeout"); return 2; }
    }
}

int luaopen_vaporui(lua_State *L)
{
    static const luaL_Reg methods[] = {
        {"set", s_set}, {"puts", s_puts}, {"fill", s_fill}, {"clear", s_clear}, {"flush", s_flush},
        {"size", s_size}, {"resize", s_resize}, {"close", s_close}, {NULL, NULL}};
    static const luaL_Reg funcs[] = {
        {"open", l_open}, {"close", l_close}, {"poll", l_poll}, {"size", l_size}, {"mouse", l_mouse}, {"now", l_now},
        {"screen", l_screen}, {"rgb", l_rgb}, {"color256", l_color256}, {"width", l_width}, {"depth", l_depth}, {NULL, NULL}};
    static const char *const names[] = {"default", "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white", "gray",
                                        "bright_red", "bright_green", "bright_yellow", "bright_blue", "bright_magenta", "bright_cyan", "bright_white", NULL};
    int i;

    if (luaL_newmetatable(L, SCREEN_MT)) {
        lua_newtable(L);
        luaL_setfuncs(L, methods, 0);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, s_close);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);
    luaL_newlib(L, funcs);
    lua_newtable(L);
    for (i = 0; names[i] != NULL; i++) { lua_pushinteger(L, i); lua_setfield(L, -2, names[i]); }
    lua_setfield(L, -2, "color");
    /* restore the terminal when the Lua state closes (os.exit, normal end), so a crash does not leave it in raw mode */
    lua_newuserdata(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, l_close);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "_guard");
    return 1;
}
