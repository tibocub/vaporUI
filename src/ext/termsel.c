#include "termsel.h"

long ts_scroll(long off, long delta, long max)
{
    off += delta;
    return off < 0 ? 0 : off > max ? max : off;
}

long ts_row_id(long base, long sb_len, long off, int y)
{
    return base + sb_len - off + y;
}

int ts_cmp(TsPos a, TsPos b)
{
    return a.line != b.line ? (a.line < b.line ? -1 : 1) : a.col < b.col ? -1 : a.col > b.col;
}

int ts_span(TsPos a, TsPos b, long id, int width, int *c0, int *c1)
{
    if (ts_cmp(a, b) > 0) { TsPos t = a; a = b; b = t; }
    if (id < a.line || id > b.line) return 0;

    *c0 = id == a.line ? a.col : 0;
    *c1 = id == b.line ? b.col + 1 : width;
    if (*c0 < 0) *c0 = 0;
    if (*c1 > width) *c1 = width;
    return *c0 < *c1;
}
