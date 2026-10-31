/* view.h — zoom and screen<->world maths for the main view.
 *
 * The original draws the world at exactly one scale (8px cells, 36px
 * floors). The port adds whole-number zoom so the 1994 art never softens:
 * the world pass renders into a VIRTUAL viewport of view_w x view_h world
 * pixels, which is then blitted to the real screen at `zoom` device pixels
 * per world pixel (nearest-neighbour). UI windows draw afterwards at 1x.
 *
 * Coordinates:
 *   screen   — real window pixels, what mouse/touch events report
 *   virtual  — pixels of the virtual viewport (screen / zoom)
 *   world    — the camera's space; cam_fx/cam_fy is the world point at the
 *              CENTRE of the virtual viewport (the port's existing camera)
 *
 * Pure and SDL-free so tests/test_sim.c can check the round trips.
 */
#ifndef VIEW_H
#define VIEW_H

#include <math.h>

/* Whole multiples up, a whole divisor down. Index VIEW_ZOOM_DEFAULT is 1x. */
static const float VIEW_ZOOM_STEPS[] = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };
#define VIEW_ZOOM_COUNT   (int)(sizeof(VIEW_ZOOM_STEPS) / sizeof(VIEW_ZOOM_STEPS[0]))
#define VIEW_ZOOM_DEFAULT 1

static inline int view_clamp_zoom_index(int i)
{
    return i < 0 ? 0 : i >= VIEW_ZOOM_COUNT ? VIEW_ZOOM_COUNT - 1 : i;
}

/* Virtual viewport extent for a real screen extent: enough world pixels
 * to cover every screen pixel (rounded up, so a 0.5x step never leaves a
 * blank edge row and a 3x step covers the partial last pixel). */
static inline int view_virtual_extent(int screen_extent, float zoom)
{
    return (int)ceilf((float)screen_extent / zoom);
}

/* Real screen pixel -> virtual viewport pixel (floor, so negative stays
 * consistent at 0.5x). */
static inline int view_screen_to_virtual(int s, float zoom)
{
    return (int)floorf((float)s / zoom);
}

/* Zoom about a fixed screen point: returns the new camera centre so the
 * world point under (sx, sy) stays under it. All in floats; the caller
 * clamps afterwards. */
static inline void view_zoom_about(float cam_x, float cam_y,
                                   int screen_w, int screen_h,
                                   float old_zoom, float new_zoom,
                                   int sx, int sy,
                                   float *out_x, float *out_y)
{
    /* half-extents as ints, matching grid_to_screen's `view_w / 2` */
    int old_hw = view_virtual_extent(screen_w, old_zoom) / 2;
    int old_hh = view_virtual_extent(screen_h, old_zoom) / 2;
    int new_hw = view_virtual_extent(screen_w, new_zoom) / 2;
    int new_hh = view_virtual_extent(screen_h, new_zoom) / 2;
    /* world point under the cursor: world = virtual + (int)cam - view/2 */
    float wx = (float)sx / old_zoom + cam_x - (float)old_hw;
    float wy = (float)sy / old_zoom + cam_y - (float)old_hh;
    *out_x = wx - (float)sx / new_zoom + (float)new_hw;
    *out_y = wy - (float)sy / new_zoom + (float)new_hh;
}

#endif /* VIEW_H */
