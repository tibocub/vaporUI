/* Windows twin of tests/pty/run.py: runs a command inside a real pseudo console (ConPTY, the conhost that Windows
   Terminal uses), feeds it keystrokes as VT bytes and prints everything it wrote.
   usage: conpty COLS ROWS COMMAND...   steps on stdin, one per line:
     send TEXT      TEXT with \e \r \n \t \\ and \xHH escapes
     wait MS
     resize COLS ROWS
   After the last step it waits up to 3 s for the program to exit, prints its output, then "EXIT <code>". */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char out[1 << 20];
static volatile size_t outn;
static HANDLE hout_r;

static DWORD WINAPI reader(void *arg)
{
    DWORD n;
    (void)arg;
    while (outn < sizeof out - 4096 && ReadFile(hout_r, out + outn, 4096, &n, NULL) && n) outn += n;
    return 0;
}

static void unescape(const char *s, char *d, size_t *len)
{
    size_t k = 0;
    for (; *s; s++) {
        if (*s != '\\') { d[k++] = *s; continue; }
        s++;
        switch (*s) {
        case 'e': d[k++] = 27; break;
        case 'r': d[k++] = '\r'; break;
        case 'n': d[k++] = '\n'; break;
        case 't': d[k++] = '\t'; break;
        case 'x': d[k++] = (char)strtol((char[]){s[1], s[2], 0}, NULL, 16); s += 2; break;
        default: d[k++] = *s;
        }
    }
    *len = k;
}

int main(int argc, char **argv)
{
    HANDLE in_r, in_w, out_w;
    HPCON pc;
    STARTUPINFOEXW si = {0};
    PROCESS_INFORMATION pi;
    SIZE_T asz = 0;
    wchar_t cmd[2048] = L"";
    char line[1024];
    COORD sz;
    DWORD code = 0;
    int i;

    if (argc < 4) { fprintf(stderr, "usage: conpty COLS ROWS COMMAND...\n"); return 2; }
    sz.X = (SHORT)atoi(argv[1]);
    sz.Y = (SHORT)atoi(argv[2]);
    for (i = 3; i < argc; i++) {
        wchar_t w[512];
        MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, w, 512);
        wcscat(cmd, i > 3 ? L" " : L"");
        wcscat(cmd, w);
    }
    SetConsoleCtrlHandler(NULL, FALSE); /* a parent shell may have set "ignore Ctrl+C", inherited by the child */
    CreatePipe(&in_r, &in_w, NULL, 0);
    CreatePipe(&hout_r, &out_w, NULL, 0);
    if (FAILED(CreatePseudoConsole(sz, in_r, out_w, 0, &pc))) { fprintf(stderr, "no ConPTY (needs Windows 10 1809+)\n"); return 2; }
    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES; /* null handles: else a redirected stdin of ours leaks into the child */
    InitializeProcThreadAttributeList(NULL, 1, 0, &asz);
    si.lpAttributeList = malloc(asz);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &asz);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof pc, NULL, NULL);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi)) {
        fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        return 2;
    }
    CreateThread(NULL, 0, reader, NULL, 0, NULL);

    Sleep(500);
    while (fgets(line, sizeof line, stdin)) {
        char buf[1024];
        size_t n;

        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, "send ", 5) == 0) {
            DWORD w;
            unescape(line + 5, buf, &n);
            WriteFile(in_w, buf, (DWORD)n, &w, NULL);
        } else if (strncmp(line, "wait ", 5) == 0) {
            Sleep((DWORD)atoi(line + 5));
        } else if (strncmp(line, "resize ", 7) == 0) {
            int c, r;
            if (sscanf(line + 7, "%d %d", &c, &r) == 2) { sz.X = (SHORT)c; sz.Y = (SHORT)r; ResizePseudoConsole(pc, sz); }
        }
        Sleep(200); /* let the program react before the next step, as run.py does */
    }
    if (WaitForSingleObject(pi.hProcess, 3000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 99); code = 99; }
    else GetExitCodeProcess(pi.hProcess, &code);
    Sleep(300);
    ClosePseudoConsole(pc);
    fwrite(out, 1, outn, stdout);
    printf("\nEXIT %lu\n", code);
    return 0;
}
