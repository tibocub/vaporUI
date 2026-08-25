#ifndef VAPORUI_H
#define VAPORUI_H


// The program can't see VuiContext's content
// to allow internal modifications without breaking
// the program.

typedef struct VuiCtx VuiCtx;


typedef struct {
    unsigned int width;
    unsigned int height;
} VuiSize;


// Initialize VaporUI.
//
// Returns:
//   A pointer to a new VuiCtx on success.
//   NULL on failure.

VuiCtx*vui_init(void);


// Shut down VaporUI and release resources.
void vui_shutdown(VuiCtx *ctx);


// Ask the terminal for its current size.
//
// Returns:
//   0  on success
//  -1  on failure

int vui_get_size(VuiCtx *ctx, VuiSize *size);


// Clear the terminal.
void vui_clear(VuiCtx *ctx);


#endif
