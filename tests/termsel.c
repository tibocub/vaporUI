// Terminal scroll/selection arithmetic. `make test`.
#include <assert.h>
#include <stdio.h>
#include "ext/termsel.h"

int main(void)
{
    int c0, c1;

    assert(ts_scroll(0, 3, 10) == 3 && ts_scroll(9, 3, 10) == 10 && ts_scroll(2, -5, 10) == 0 && ts_scroll(0, 5, 0) == 0);
    // 100 dropped, 50 kept, 24 rows: live view row 0 is the first screen row; 10 back shows the last 10 kept lines
    assert(ts_row_id(100, 50, 0, 0) == 150 && ts_row_id(100, 50, 10, 0) == 140 && ts_row_id(100, 50, 50, 0) == 100);

    TsPos a = { 5, 10 }, b = { 7, 3 };
    assert(ts_span(a, b, 4, 80, &c0, &c1) == 0 && ts_span(a, b, 8, 80, &c0, &c1) == 0);
    assert(ts_span(a, b, 5, 80, &c0, &c1) && c0 == 10 && c1 == 80);
    assert(ts_span(a, b, 6, 80, &c0, &c1) && c0 == 0 && c1 == 80);
    assert(ts_span(a, b, 7, 80, &c0, &c1) && c0 == 0 && c1 == 4);
    assert(ts_span(b, a, 7, 80, &c0, &c1) && c1 == 4);            // order doesn't matter
    assert(ts_span(a, (TsPos){ 5, 12 }, 5, 80, &c0, &c1) && c0 == 10 && c1 == 13);
    assert(ts_span((TsPos){ 5, 70 }, (TsPos){ 5, 90 }, 5, 40, &c0, &c1) == 0); // narrower line than the selection
    assert(ts_span((TsPos){ 5, 20 }, (TsPos){ 6, 2 }, 5, 10, &c0, &c1) == 0);   // wide start col past a narrow line's end

    puts("termsel: ok");
    return 0;
}
