/* touch.h — touch gestures for mobile play (port addition).
 *
 * Ported from BrandyTower's src/input/pointer.ts. Building and panning are
 * both drags, so a single touch starts PENDING and only becomes a drag once
 * it has moved far enough to mean it; whether that drag builds or pans is
 * the game's call (claims_drag). A second finger cancels it into a pinch,
 * which zooms in whole steps and pans with its midpoint. A touch held still
 * long enough is a long-press (tooltip), never a build.
 *
 * Pure and SDL-free: coordinates are logical canvas pixels, time is ms.
 * main.c turns the callbacks into the mouse events the game already handles.
 */
#ifndef TOUCH_H
#define TOUCH_H

#define TOUCH_DRAG_THRESHOLD 12     /* px a touch may wander and stay a tap */
#define TOUCH_LONG_PRESS_MS  500
#define TOUCH_PINCH_STEP     1.5f   /* distance ratio per zoom step */
#define TOUCH_MAX            2      /* fingers tracked; more are ignored */

typedef struct {
    void *ctx;
    void (*tap)(void *ctx, int x, int y);
    void (*drag_start)(void *ctx, int x, int y);
    void (*drag_move)(void *ctx, int x, int y);
    void (*drag_end)(void *ctx, int x, int y);
    /* A drag interrupted by a second finger: undo/abandon, don't commit. */
    void (*drag_cancel)(void *ctx);
    void (*pan)(void *ctx, float dx, float dy);
    void (*zoom_step)(void *ctx, int dir, int cx, int cy);
    void (*long_press)(void *ctx, int x, int y);
    /* Does a drag starting here belong to the active tool (build, drag a
     * window, pull a shaft cap) rather than panning the view? */
    int  (*claims_drag)(void *ctx, int x, int y);
} TouchHandlers;

typedef enum {
    TP_PENDING = 0,   /* undecided: tap, drag, pan or long-press */
    TP_DRAGGING,      /* drawing on the tower / dragging UI */
    TP_PANNING,
    TP_DONE           /* consumed (long-press fired, or left a pinch) */
} TouchPhase;

typedef struct {
    long long  id;
    int        active;
    float      x, y, start_x, start_y;
    unsigned   down_ms;
    TouchPhase phase;
} TouchPoint;

typedef struct {
    TouchPoint pt[TOUCH_MAX];
    int        count;
    int        pinching;
    float      pinch_ref;   /* finger distance at the last zoom step */
    float      mid_x, mid_y;
} TouchState;

void touch_reset(TouchState *s);
void touch_down(TouchState *s, const TouchHandlers *h, long long id,
                float x, float y, unsigned ms);
void touch_move(TouchState *s, const TouchHandlers *h, long long id,
                float x, float y);
void touch_up(TouchState *s, const TouchHandlers *h, long long id,
              float x, float y);
/* Call every frame: fires the long-press timer. */
void touch_tick(TouchState *s, const TouchHandlers *h, unsigned ms);
/* Any finger down? (the game hides hover-only affordances while touched) */
static inline int touch_active(const TouchState *s) { return s->count > 0; }

#endif /* TOUCH_H */
