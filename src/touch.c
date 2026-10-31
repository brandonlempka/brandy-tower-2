/* touch.c — see touch.h. Ported from BrandyTower src/input/pointer.ts. */
#include "touch.h"
#include <math.h>
#include <string.h>

void touch_reset(TouchState *s)
{
    memset(s, 0, sizeof *s);
}

static TouchPoint *find(TouchState *s, long long id)
{
    for (int i = 0; i < TOUCH_MAX; i++)
        if (s->pt[i].active && s->pt[i].id == id) return &s->pt[i];
    return NULL;
}

static float finger_distance(const TouchState *s)
{
    const TouchPoint *a = &s->pt[0], *b = &s->pt[1];
    return hypotf(a->x - b->x, a->y - b->y);
}

static void midpoint(const TouchState *s, float *x, float *y)
{
    *x = (s->pt[0].x + s->pt[1].x) / 2.0f;
    *y = (s->pt[0].y + s->pt[1].y) / 2.0f;
}

static void start_pinch(TouchState *s, const TouchHandlers *h)
{
    /* A second finger means the first was never a tap or a build drag:
     * abandon a drag in progress rather than committing it. */
    for (int i = 0; i < TOUCH_MAX; i++) {
        TouchPoint *p = &s->pt[i];
        if (!p->active) continue;
        if (p->phase == TP_DRAGGING && h->drag_cancel) h->drag_cancel(h->ctx);
        p->phase = TP_DONE;
    }
    s->pinching = 1;
    s->pinch_ref = finger_distance(s);
    midpoint(s, &s->mid_x, &s->mid_y);
}

void touch_down(TouchState *s, const TouchHandlers *h, long long id,
                float x, float y, unsigned ms)
{
    if (find(s, id) || s->count >= TOUCH_MAX) return;   /* a third finger */
    TouchPoint *p = NULL;
    for (int i = 0; i < TOUCH_MAX; i++)
        if (!s->pt[i].active) { p = &s->pt[i]; break; }
    if (!p) return;
    *p = (TouchPoint){ .id = id, .active = 1, .x = x, .y = y,
                       .start_x = x, .start_y = y, .down_ms = ms,
                       .phase = TP_PENDING };
    s->count++;
    /* keep the two live fingers in slots 0 and 1 for the pinch maths */
    if (s->count == 2 && !s->pt[0].active) {
        s->pt[0] = s->pt[1];
        s->pt[1].active = 0;
    }
    if (s->count == 2) start_pinch(s, h);
}

void touch_move(TouchState *s, const TouchHandlers *h, long long id,
                float x, float y)
{
    TouchPoint *p = find(s, id);
    if (!p) return;
    float dx = x - p->x, dy = y - p->y;
    p->x = x;
    p->y = y;

    if (s->pinching && s->count == 2) {
        float mx, my;
        midpoint(s, &mx, &my);
        /* Two fingers moving together pan as well as pinch. */
        if (h->pan) h->pan(h->ctx, mx - s->mid_x, my - s->mid_y);
        s->mid_x = mx;
        s->mid_y = my;
        float d = finger_distance(s);
        if (s->pinch_ref > 0 && d > 0) {
            /* whole steps only, so the art never softens: one step each
             * time the spread grows or shrinks by TOUCH_PINCH_STEP */
            while (d / s->pinch_ref >= TOUCH_PINCH_STEP) {
                s->pinch_ref *= TOUCH_PINCH_STEP;
                if (h->zoom_step) h->zoom_step(h->ctx, +1, (int)mx, (int)my);
            }
            while (s->pinch_ref / d >= TOUCH_PINCH_STEP) {
                s->pinch_ref /= TOUCH_PINCH_STEP;
                if (h->zoom_step) h->zoom_step(h->ctx, -1, (int)mx, (int)my);
            }
        }
        return;
    }

    switch (p->phase) {
    case TP_PENDING: {
        float moved = hypotf(x - p->start_x, y - p->start_y);
        if (moved < TOUCH_DRAG_THRESHOLD) return;
        int sx = (int)p->start_x, sy = (int)p->start_y;
        if (h->claims_drag && h->claims_drag(h->ctx, sx, sy)) {
            p->phase = TP_DRAGGING;
            if (h->drag_start) h->drag_start(h->ctx, sx, sy);
            if (h->drag_move) h->drag_move(h->ctx, (int)x, (int)y);
        } else {
            p->phase = TP_PANNING;
            /* catch up the threshold distance so the view doesn't lag */
            if (h->pan) h->pan(h->ctx, x - p->start_x, y - p->start_y);
        }
        break;
    }
    case TP_DRAGGING:
        if (h->drag_move) h->drag_move(h->ctx, (int)x, (int)y);
        break;
    case TP_PANNING:
        if (h->pan) h->pan(h->ctx, dx, dy);
        break;
    case TP_DONE:
        break;
    }
}

void touch_up(TouchState *s, const TouchHandlers *h, long long id,
              float x, float y)
{
    TouchPoint *p = find(s, id);
    if (!p) return;
    p->x = x;
    p->y = y;
    TouchPhase phase = p->phase;
    p->active = 0;
    s->count--;

    if (phase == TP_PENDING && !s->pinching) {
        if (h->tap) h->tap(h->ctx, (int)p->start_x, (int)p->start_y);
    } else if (phase == TP_DRAGGING) {
        if (h->drag_end) h->drag_end(h->ctx, (int)x, (int)y);
    }

    if (s->count == 1 && s->pinching) {
        /* Coming out of a pinch: the remaining finger pans rather than
         * suddenly building. */
        s->pinching = 0;
        for (int i = 0; i < TOUCH_MAX; i++)
            if (s->pt[i].active) {
                s->pt[i].phase = TP_PANNING;
                if (i != 0) { s->pt[0] = s->pt[i]; s->pt[i].active = 0; }
            }
    }
    if (s->count == 0) s->pinching = 0;
}

void touch_tick(TouchState *s, const TouchHandlers *h, unsigned ms)
{
    if (s->count != 1 || s->pinching) return;
    for (int i = 0; i < TOUCH_MAX; i++) {
        TouchPoint *p = &s->pt[i];
        if (!p->active || p->phase != TP_PENDING) continue;
        if (ms - p->down_ms >= TOUCH_LONG_PRESS_MS) {
            p->phase = TP_DONE;          /* released later: no tap */
            if (h->long_press) h->long_press(h->ctx, (int)p->x, (int)p->y);
        }
    }
}
