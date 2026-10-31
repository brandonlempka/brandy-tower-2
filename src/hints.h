/* hints.h — tooltip and explainer text (port addition).
 *
 * Ported from BrandyTower's toolbar tooltips, rating tooltip and the
 * inspector's advice notes. The faithful UI says WHAT ("Conditions are
 * terrible", a greyed menu item, three stars); these say WHY and HOW TO FIX
 * IT, so a red word is never a dead end.
 *
 * Every line is read off the same state and functions the simulation
 * decides with (the promotion needs list, tower_pool_full, the judge's
 * metric and bars, tower.c's own refusal wording) — never restated — so a
 * hint cannot promise a fix the game does not honour.
 *
 * SDL-free; tests/test_sim.c checks it.
 */
#ifndef HINTS_H
#define HINTS_H

#include "game.h"

#define HINT_LEN   192   /* the renderer word-wraps */
#define HINT_LINES 28   /* the full star ladder is ~22 */

/* Toolbox / Build-menu item: name and price, why it is unavailable (if it
 * is), where it may go, and what it is for on the star ladder. */
int hints_tool(const GameSim *sim, const Tower *tower, ItemType type,
               char lines[][HINT_LEN], int max);

/* Why an item can't be picked right now, or NULL if it can. */
const char *hints_unavailable(const GameSim *sim, const Tower *tower,
                              ItemType type);

/* The star rating: every rating still to come and what each needs, ticked
 * against the tower as it stands. */
int hints_rating(const GameSim *sim, const Tower *tower,
                 char lines[][HINT_LEN], int max);

/* One line: what the very next rating is still missing. */
void hints_next_rating(const GameSim *sim, const Tower *tower,
                       char *buf, int bufn);

/* A unit's notes: what its status means and what to do about it, most
 * urgent first. */
int hints_tenant_notes(GameSim *sim, Tower *tower, const Tenant *t,
                       char lines[][HINT_LEN], int max);

#endif /* HINTS_H */
