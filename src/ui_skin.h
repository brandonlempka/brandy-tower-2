/* ui_skin.h — the optional "Modern UI" skin (port addition, 2026-10-31).
 *
 * The faithful chrome is Win3.1: grey bevels, navy title bars, black text.
 * Options -> Modern UI swaps it for dark glass — translucent rounded panels,
 * soft shadows, light text, an accent blue — WITHOUT moving a single rect:
 * every window keeps its layout and hit-test code, only the paint changes.
 *
 * Two layers do that:
 *
 *  1. Hand-drawn modern versions of the shared primitives (title bars,
 *     panels, buttons, fields, gauges) and the bitmap-backed surfaces
 *     (info bar, toolbox, map tabs), branching on ui_skin_modern().
 *
 *  2. A colour remap for everything else. While the UI pass is drawing
 *     (ui_chrome_begin/end) and the skin is modern, main.c routes
 *     SDL_SetRenderDrawColor through ui_remap_fill and every TTF colour
 *     through ui_ink: opaque Win3.1 greys become the dark-glass shades and
 *     dark text becomes light, so the many inline-drawn dialog parts follow
 *     the skin without each being rewritten. Translucent fills (shadows,
 *     screen dims) and saturated colours pass through untouched. Content
 *     that must keep its true colours (the minimap picture, the tenant
 *     picture, the bitmap-backed elevator and finance dialogs) is drawn
 *     inside ui_remap_suspend/resume.
 *
 * In the classic skin every function here is a no-op or identity, so the
 * faithful frame is byte-identical.
 */
#ifndef UI_SKIN_H
#define UI_SKIN_H

#include <SDL.h>

typedef struct {
    SDL_Color panel;       /* glass body */
    SDL_Color header;      /* glass title strip */
    SDL_Color raised;      /* buttons, raised wells */
    SDL_Color raised_hot;  /* hovered / pressed button */
    SDL_Color field;       /* inset text field */
    SDL_Color border;      /* 1px panel edge */
    SDL_Color highlight;   /* 1px top sheen */
    SDL_Color text;
    SDL_Color text_dim;
    SDL_Color text_off;    /* disabled */
    SDL_Color accent;      /* selection, default button */
    SDL_Color accent_soft; /* selected-but-quiet fills */
    SDL_Color gold;        /* stars */
    SDL_Color good, ok, bad;
} UiPalette;

extern const UiPalette UI;

enum {
    UI_TL = 1, UI_TR = 2, UI_BR = 4, UI_BL = 8,
    UI_TOP = UI_TL | UI_TR, UI_BOTTOM = UI_BL | UI_BR,
    UI_ALL = UI_TOP | UI_BOTTOM
};

enum { UI_BTN_NORMAL = 0, UI_BTN_HOT, UI_BTN_ON, UI_BTN_ACCENT, UI_BTN_OFF };

#define UI_RADIUS 8

void ui_skin_init(SDL_Renderer *r);
void ui_skin_set(int modern);
int  ui_skin_modern(void);

/* Frame bookkeeping: call ui_frame_begin once per frame before drawing. */
void ui_frame_begin(void);

/* The colour remap (layer 2). */
void ui_chrome_begin(void);
void ui_chrome_end(void);
void ui_remap_suspend(void);
void ui_remap_resume(void);
int  ui_remap_active(void);   /* modern, in the UI pass, not suspended */
void ui_remap_fill(Uint8 *r, Uint8 *g, Uint8 *b, Uint8 a);
SDL_Color ui_ink(SDL_Color c);

/* Primitives (layer 1). Colours are drawn exactly, never remapped. */
void ui_fill_round(SDL_Rect r, int rad, int corners, SDL_Color c);
void ui_stroke_round(SDL_Rect r, int rad, int corners, SDL_Color c);
void ui_shadow(SDL_Rect r, int rad);
void ui_panel(SDL_Rect r, int rad, int corners);
void ui_button(SDL_Rect r, int state);
void ui_field(SDL_Rect r);
void ui_close_box(SDL_Rect r);
void ui_star(int cx, int cy, int radius, int filled);
void ui_progress(SDL_Rect r, int permille, SDL_Color fill);

/* Window title strips: a body drawn directly under a header this frame (or
 * last) rounds only its bottom corners, so header + body read as one
 * panel whatever order they were painted in. */
void ui_header_note(SDL_Rect header);
int  ui_header_above(SDL_Rect body);

#endif
