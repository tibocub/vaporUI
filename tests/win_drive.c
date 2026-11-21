/* Types real keystrokes (SendInput, by scan code) into a real console window and reads back what the program drew.
   usage: win_drive [KLID] COMMAND...      chords on stdin, one per line:  Ctrl+Alt+é   Alt+Shift+&   F5   Ctrl+Shift+Left
   KLID (optional, 8 hex digits, e.g. 0000040C for French AZERTY) is the keyboard layout asked of the console window.
   Keys are named by their AZERTY keycap (é is the key left of 3, & the key left of é...), so the same names are the same physical
   keys under any layout: the layout decides what Windows makes of them, which is the point. Prints "CHORD | status line".
   Needs the foreground: it checks before every chord and stops rather than type into another window.  Windows only. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct { const wchar_t *name; WORD sc; int ext; } Key;

static const Key keys[] = {
    {L"&", 0x02}, {L"é", 0x03}, {L"\"", 0x04}, {L"'", 0x05}, {L"(", 0x06}, {L"-", 0x07}, {L"è", 0x08}, {L"_", 0x09},
    {L"ç", 0x0A}, {L"à", 0x0B}, {L")", 0x0C}, {L"=", 0x0D}, {L"Backspace", 0x0E}, {L"Tab", 0x0F},
    {L"a", 0x10}, {L"z", 0x11}, {L"e", 0x12}, {L"r", 0x13}, {L"t", 0x14}, {L"y", 0x15}, {L"u", 0x16}, {L"i", 0x17},
    {L"o", 0x18}, {L"p", 0x19}, {L"^", 0x1A}, {L"$", 0x1B}, {L"Enter", 0x1C},
    {L"q", 0x1E}, {L"s", 0x1F}, {L"d", 0x20}, {L"f", 0x21}, {L"g", 0x22}, {L"h", 0x23}, {L"j", 0x24}, {L"k", 0x25},
    {L"l", 0x26}, {L"m", 0x27}, {L"ù", 0x28}, {L"*", 0x2B},
    {L"<", 0x56}, {L"w", 0x2C}, {L"x", 0x2D}, {L"c", 0x2E}, {L"v", 0x2F}, {L"b", 0x30}, {L"n", 0x31}, {L",", 0x32},
    {L";", 0x33}, {L":", 0x34}, {L"!", 0x35}, {L"Space", 0x39}, {L"Escape", 0x01},
    {L"F1", 0x3B}, {L"F2", 0x3C}, {L"F3", 0x3D}, {L"F4", 0x3E}, {L"F5", 0x3F}, {L"F6", 0x40}, {L"F7", 0x41}, {L"F8", 0x42},
    {L"F9", 0x43}, {L"F10", 0x44}, {L"F11", 0x57}, {L"F12", 0x58},
    {L"Home", 0x47, 1}, {L"Up", 0x48, 1}, {L"PageUp", 0x49, 1}, {L"Left", 0x4B, 1}, {L"Right", 0x4D, 1}, {L"End", 0x4F, 1},
    {L"Down", 0x50, 1}, {L"PageDown", 0x51, 1}, {L"Insert", 0x52, 1}, {L"Delete", 0x53, 1},
};

static HWND hwnd;

static void key(WORD sc, int ext, int up)
{
    INPUT in = {0};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = sc;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (ext ? KEYEVENTF_EXTENDEDKEY : 0) | (up ? KEYEVENTF_KEYUP : 0);
    SendInput(1, &in, sizeof in);
    Sleep(20);
}

static int foreground_ok(void) { return GetForegroundWindow() == hwnd; }

static void focus(void)
{
    DWORD fg = GetWindowThreadProcessId(GetForegroundWindow(), NULL), me = GetCurrentThreadId();
    AttachThreadInput(me, fg, TRUE);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    AttachThreadInput(me, fg, FALSE);
    Sleep(300);
}

/* the program's status line: row 1 from column 20, trailing blanks cut */
static void status(char *out, size_t cap)
{
    HANDLE h = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    wchar_t w[200];
    DWORD n = 0;
    COORD at = {20, 1};
    int len;

    out[0] = 0;
    if (h == INVALID_HANDLE_VALUE) return;
    if (ReadConsoleOutputCharacterW(h, w, 120, at, &n)) {
        while (n > 0 && w[n - 1] == L' ') n--;
        w[n] = 0;
        len = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
        if (len <= 0) out[0] = 0;
        { char *c = strstr(out, "   "); if (c) *c = 0; }
    }
    CloseHandle(h);
}

static const Key *find(const wchar_t *name)
{
    size_t i;
    for (i = 0; i < sizeof keys / sizeof *keys; i++) if (_wcsicmp(keys[i].name, name) == 0) return &keys[i];
    return NULL;
}

int main(int argc, char **argv)
{
    wchar_t cmd[1024] = L"";
    char line[512], st[400];
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    int i = 1;
    const char *klid = NULL;

    if (argc > 1 && strlen(argv[1]) == 8 && strspn(argv[1], "0123456789abcdefABCDEF") == 8) klid = argv[i++];
    if (i >= argc) { fprintf(stderr, "usage: win_drive [KLID] COMMAND...\n"); return 2; }
    for (; i < argc; i++) {
        wchar_t a[512];
        MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, a, 512);
        wcscat(cmd, cmd[0] ? L" " : L"");
        wcscat(cmd, a);
    }
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) { fprintf(stderr, "CreateProcess: %lu\n", GetLastError()); return 2; }
    Sleep(1500);
    FreeConsole();
    if (!AttachConsole(pi.dwProcessId)) { fprintf(stderr, "AttachConsole: %lu\n", GetLastError()); return 2; }
    hwnd = GetConsoleWindow();
    if (klid) {
        wchar_t k[16];
        MultiByteToWideChar(CP_UTF8, 0, klid, -1, k, 16);
        PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)LoadKeyboardLayoutW(k, KLF_ACTIVATE));
        Sleep(500);
    }
    focus();
    {
        HKL l = GetKeyboardLayout(GetWindowThreadProcessId(hwnd, NULL));
        printf("# window layout %08lx, foreground %s\n", (unsigned long)(UINT_PTR)l, foreground_ok() ? "ok" : "NOT ok");
        fflush(stdout);
    }
    if (!foreground_ok()) { TerminateProcess(pi.hProcess, 1); return 3; }

    while (fgets(line, sizeof line, stdin)) {
        wchar_t w[256], *p, *tok;
        int ctrl = 0, alt = 0, shift = 0, altgr = 0;
        const Key *k;
        char before[400];
        int nosent;

        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        nosent = line[0] == 0x3e; /* ">chord": no sentinel (a dead key must not see Escape before its partner) */
        MultiByteToWideChar(CP_UTF8, 0, line + nosent, -1, w, 256);
        /* modifiers: everything before the last '+' that is not the key itself ("Ctrl++" is not supported: use Ctrl+= ) */
        p = w;
        for (;;) {
            tok = wcschr(p, L'+');
            if (!tok || tok == p || !tok[1]) break;
            *tok = 0;
            if (!_wcsicmp(p, L"Ctrl")) ctrl = 1; else if (!_wcsicmp(p, L"Alt")) alt = 1;
            else if (!_wcsicmp(p, L"Shift")) shift = 1; else if (!_wcsicmp(p, L"AltGr")) altgr = 1;
            p = tok + 1;
        }
        k = find(p);
        if (!k) { printf("%s | ?? unknown key\n", line); continue; }
        if (!foreground_ok()) { printf("ABORT: lost the foreground, nothing more typed\n"); break; }

        if (!nosent) { key(0x01, 0, 0); key(0x01, 0, 1); Sleep(150); } /* sentinel: Escape, so "no event" is visible */
        status(before, sizeof before);
        if (ctrl) key(0x1D, 0, 0);
        if (alt) key(0x38, 0, 0);
        if (altgr) { key(0x1D, 0, 0); key(0x38, 1, 0); }
        if (shift) key(0x2A, 0, 0);
        key(k->sc, k->ext, 0); key(k->sc, k->ext, 1);
        if (shift) key(0x2A, 0, 1);
        if (altgr) { key(0x38, 1, 1); key(0x1D, 0, 1); }
        if (alt) key(0x38, 0, 1);
        if (ctrl) key(0x1D, 0, 1);
        Sleep(200);
        status(st, sizeof st);
        printf("%-22s | %s\n", line, strcmp(st, before) == 0 ? "(no event: the console or program ate it)" : st);
        fflush(stdout);
    }
    key(0x1D, 0, 0); key(0x2E, 0, 0); key(0x2E, 0, 1); key(0x1D, 0, 1); /* Ctrl+C quits the example */
    if (WaitForSingleObject(pi.hProcess, 1500) != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
    return 0;
}
