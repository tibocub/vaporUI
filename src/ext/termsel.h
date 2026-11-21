// Pure arithmetic of the terminal widget's scroll view and selection (no libvterm, tested by tests/termsel.c).
// A line has an id that stays with its content: scrollback line i (0 = oldest kept) is base + i and screen row r
// is base + sb_len + r, where `base` counts the lines dropped off the oldest end.
#ifndef VUI_TERMSEL_H
#define VUI_TERMSEL_H

typedef struct { long line; int col; } TsPos;

long ts_scroll(long off, long delta, long max);                  // off + delta clamped to 0..max
long ts_row_id(long base, long sb_len, long off, int y);         // id of view row y when scrolled `off` lines back
int ts_cmp(TsPos a, TsPos b);
// Cells of line `id` (width cells) covered by the selection a..b (either order, both ends inclusive).
// Returns 0 if none, else the half-open range [*c0, *c1).
int ts_span(TsPos a, TsPos b, long id, int width, int *c0, int *c1);

#endif
