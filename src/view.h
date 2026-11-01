/* view.h — zoom and screen<->world maths for the main view.
 *
 * The original draws the world at exactly one scale (8px cells, 36px
 * floors). The port adds zoom: the world pass renders into a VIRTUAL
 * viewport of view_w x view_h world pixels, which is then blitted to the
 * real screen at `zoom` logical pixels per world pixel. When that lands on
 * a whole number of device pixels the blit is nearest-neighbour, so the
 * 1994 art never softens; the in-between steps (needed because 0.5x -> 1x
 * is too big a jump) are filtered instead (view_zoom_is_crisp). UI windows
 * draw afterwards at 1x.
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

/* Fine steps through the zoomed-out range (where the jump from 0.5x to 1x
 * was too coarse), 1.5x (whole device pixels on a 2x screen), then whole
 * multiples. Every step is a multiple of 1/8, so on a 2x screen 0.5x and
 * 1.5x stay crisp too. Index VIEW_ZOOM_DEFAULT is 1x. */
static const float VIEW_ZOOM_STEPS[] = {
    0.5f, 0.625f, 0.75f, 0.875f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f
};
#define VIEW_ZOOM_COUNT   (int)(sizeof(VIEW_ZOOM_STEPS) / sizeof(VIEW_ZOOM_STEPS[0]))
#define VIEW_ZOOM_DEFAULT 4

static inline int view_clamp_zoom_index(int i)
{
    return i < 0 ? 0 : i >= VIEW_ZOOM_COUNT ? VIEW_ZOOM_COUNT - 1 : i;
}

/* The step nearest a zoom factor (for --zoom=2 / CT_ZOOM=0.75). */
static inline int view_zoom_index_for(float zoom)
{
    int best = VIEW_ZOOM_DEFAULT;
    for (int i = 0; i < VIEW_ZOOM_COUNT; i++)
        if (fabsf(VIEW_ZOOM_STEPS[i] - zoom) < fabsf(VIEW_ZOOM_STEPS[best] - zoom))
            best = i;
    return best;
}

/* Does a world pixel cover a whole number (>= 1) of device pixels? Then the
 * blit is nearest-neighbour and pixel-exact; otherwise nearest would drop
 * or double odd rows (shimmering as the camera moves), so it is filtered.
 * px_scale is device px per logical px (2 on a Retina window). */
static inline int view_zoom_is_crisp(float zoom, int px_scale)
{
    float d = zoom * (float)(px_scale > 0 ? px_scale : 1);
    return d >= 1.0f && fabsf(d - roundf(d)) < 0.001f;
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
