/* overlay.c — the map views' per-tenant tint. See overlay.h. */
#include "overlay.h"
#include "game.h"

static int set_rgb(uint8_t rgb[3], uint8_t r, uint8_t g, uint8_t b)
{
    rgb[0] = r; rgb[1] = g; rgb[2] = b;
    return 1;
}

int overlay_tenant_color(const Tenant *t, int mode, uint8_t rgb[3])
{
    if (!t) return 0;
    switch (mode) {
    case MAP_VIEW_EVAL:
        /* Eval = the JUDGE'S verdict byte, exactly like the EXE
         * (FUN_11d0_0363 switches on +0x15): 0 red / 1 yellow / 2-3
         * cyan / 0xFF or never-judged = NO tint. The old live-stress
         * fallback tiers were invented (legacy-stress referee
         * 2026-08-02). Exact overlay RGBs from the 11d0:0363 colour
         * tables (referee 2026-08-08): RED(230,0,0) YELLOW(204,204,0)
         * CYAN(0,255,255). ABANDONED keeps a dim red as the port's
         * rescue-target readability aid (documented divergence). */
        if (t->state == TENANT_EMPTY || t->state == TENANT_CONSTRUCTION)
            return 0;
        if (t->state == TENANT_ABANDONED)  return set_rgb(rgb, 100, 30, 30);
        if (t->demand_category == 0)       return set_rgb(rgb, 230, 0, 0);
        if (t->demand_category == 1)       return set_rgb(rgb, 204, 204, 0);
        if (t->demand_category == 2 || t->demand_category == 3)
            return set_rgb(rgb, 0, 255, 255);
        return 0;                          /* 0xFF / unjudged: skip */
    case MAP_VIEW_RENT:
        switch (t->rent_class) {           /* mode-2 table: 0..3, 4 skip */
        case 0:  return set_rgb(rgb, 230, 0, 0);     /* High */
        default: return set_rgb(rgb, 204, 204, 0);   /* Average */
        case 2:  return set_rgb(rgb, 0, 255, 0);     /* Low */
        case 3:  return set_rgb(rgb, 0, 255, 255);   /* Very Low */
        }
    case MAP_VIEW_HOTEL:
        /* Hotel rooms with status >= dirty band -> pure RED, everything
         * else skipped (11d0 mode-3 rule: byte +0xB >= 0x28 covers dirty
         * AND infested; the old amber split was a port invention). */
        if (item_is_hotel_room(t->type) &&
            (t->condition == ROOM_DIRTY || t->condition == ROOM_INFESTED))
            return set_rgb(rgb, 230, 0, 0);
        return 0;
    default:
        return 0;
    }
}

int overlay_world_color(const Tenant *t, int mode, uint8_t rgb[3])
{
    if (!t || t->type == ITEM_NONE || t->type == ITEM_FLOOR ||
        t->type == ITEM_LOBBY || t->type == ITEM_CATHEDRAL ||
        item_is_transport(t->type))
        return 0;
    if (mode == MAP_VIEW_RENT && tenant_rent(t->type, 1) <= 0)
        return 0;
    if (!overlay_tenant_color(t, mode, rgb)) return 0;
    /* Softer inks than the minimap's pure table (BrandyTower's wash), so
     * the room art reads through: the same hue per verdict, each table
     * entry mapped one-to-one. */
    static const uint8_t soft[][6] = {
        { 230,   0,   0,  232,  52,  52 },   /* red: Terrible / High / Dirty */
        { 204, 204,   0,  226, 204,  48 },   /* yellow: Good / Average */
        {   0, 255, 255,   44, 172, 226 },   /* cyan: Excellent / Very Low */
        {   0, 255,   0,   60, 200,  90 },   /* green: Low */
    };
    for (unsigned i = 0; i < sizeof soft / sizeof soft[0]; i++)
        if (rgb[0] == soft[i][0] && rgb[1] == soft[i][1] &&
            rgb[2] == soft[i][2]) {
            rgb[0] = soft[i][3]; rgb[1] = soft[i][4]; rgb[2] = soft[i][5];
            break;
        }
    return 1;
}
