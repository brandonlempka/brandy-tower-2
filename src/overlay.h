/* overlay.h — the map views' per-tenant tint (Eval / Rent / Hotel).
 *
 * One table, two consumers: the minimap paints it faithfully (seg59
 * GetTenantOverlayColor, 11d0:0363 colour tables), and the main tower view
 * washes the same colour over each room when a map view is up — a port
 * addition carried over from BrandyTower's evaluation overlay (2026-10-31),
 * because the answer a map view gives is a SHAPE: one red room is a room, a
 * red column up the tower is a lift, a red band across a floor is the party
 * hall keeping everyone awake.
 *
 * SDL-free; tests/test_sim.c checks it.
 */
#ifndef OVERLAY_H
#define OVERLAY_H

#include <stdint.h>
#include "tower.h"

enum { MAP_VIEW_PLAIN = 0, MAP_VIEW_EVAL = 1, MAP_VIEW_RENT = 2,
       MAP_VIEW_HOTEL = 3 };

/* The minimap's colour for a non-shell tenant in map view `mode` (1-3).
 * Returns 1 and fills rgb when the view tints it; 0 when the view skips it
 * (the minimap then paints the grey silhouette). Exactly the EXE's tables. */
int overlay_tenant_color(const Tenant *t, int mode, uint8_t rgb[3]);

/* The main-view wash: the same colours, but only on rooms the view has an
 * opinion about — never floors, transports, the lobby or the cathedral,
 * and in the Rent view only units that actually pay rent (the minimap's
 * faithful "everything else is Average" fallback would wash the lobby
 * yellow). The inks are the table's hues softened so the room art reads
 * through. Returns 1 and fills rgb when the room should be washed. */
int overlay_world_color(const Tenant *t, int mode, uint8_t rgb[3]);

#endif
