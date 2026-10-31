/* hints.c — see hints.h. Ported from BrandyTower src/ui/toolbar.ts
 * (toolTooltip/unavailableReason), src/ui/infobar.ts (ratingTooltip/
 * nextRatingNote) and src/ui/advice.ts (facilityNotes), re-derived against
 * this engine's own rules. */
#include "hints.h"
#include "people.h"
#include <stdio.h>
#include <string.h>

#define PUSHF(...) do { if (n < max) { \
    snprintf(lines[n], HINT_LEN, __VA_ARGS__); n++; } } while (0)

static void money(long v, char *buf, int bufn)
{
    char raw[32];
    snprintf(raw, sizeof raw, "%ld", v < 0 ? -v : v);
    int len = (int)strlen(raw), o = 0;
    if (v < 0 && o < bufn - 1) buf[o++] = '-';
    if (o < bufn - 1) buf[o++] = '$';
    for (int i = 0; i < len && o < bufn - 1; i++) {
        buf[o++] = raw[i];
        int left = len - 1 - i;
        if (left > 0 && left % 3 == 0 && o < bufn - 1) buf[o++] = ',';
    }
    buf[o] = 0;
}

static void count_str(int v, char *buf, int bufn)
{
    money(v, buf, bufn);
    memmove(buf, buf + 1, strlen(buf));        /* drop the '$' */
}

static const char *rating_name(int star)
{
    static const char *names[] = { "", "1 star", "2 stars", "3 stars",
                                   "4 stars", "5 stars", "TOWER" };
    return (star >= 1 && star <= 6) ? names[star] : "";
}

/* ---------------------------------------------------------------- tools */

const char *hints_unavailable(const GameSim *sim, const Tower *tower,
                              ItemType type)
{
    static char buf[HINT_LEN];
    if (type <= ITEM_NONE || type >= ITEM_TYPE_COUNT) return NULL;
    if (sim->mode == MODE_CAMPAIGN &&
        ITEM_STAR_REQ[type] > tower->star_rating) {
        snprintf(buf, sizeof buf, "Unlocks at %s",
                 rating_name(ITEM_STAR_REQ[type]));
        return buf;
    }
    if (tower_pool_full(tower, type)) {
        if (item_is_elevator(type))
            return "No more elevator shafts available (24)";
        if (type == ITEM_METRO || type == ITEM_CATHEDRAL)
            return "One per tower, and you have it";
        return "The tower will take no more of these";
    }
    return NULL;
}

/* Where an item may go, in the words tower.c refuses with. */
static const char *placement_rule(ItemType type)
{
    switch (type) {
    case ITEM_LOBBY:            return "Ground floor, then a sky lobby every 15 floors";
    case ITEM_FLOOR:            return "Drag to lay a run of empty floor";
    case ITEM_ELEVATOR_SHAFT:
    case ITEM_ELEVATOR_SERVICE: return "One shaft covers up to 30 floors, down to B9";
    case ITEM_ELEVATOR_EXPRESS: return "New express shafts start at a lobby floor (every 15th)";
    case ITEM_ESCALATOR:        return "Only between commercial spaces";
    case ITEM_RECYCLING:        return "Each new center must be near an existing one";
    case ITEM_RAMP:             return "The first goes on B1; the rest stack straight below it";
    case ITEM_PARKING:          return "Needs a Parking Ramp on the same level";
    case ITEM_METRO:            return "On the bottom floor; nothing can be built under it";
    case ITEM_CATHEDRAL:        return "100th floor only";
    default:                    return NULL;
    }
}

/* What it is for, where the star ladder or a system asks for it. */
static const char *purpose(ItemType type)
{
    switch (type) {
    case ITEM_SECURITY:     return "Needed for 3 stars; guards answer fires and bombs";
    case ITEM_HOTEL_SUITE:  return "One is needed for 4 stars (and the VIP)";
    case ITEM_MEDICAL:      return "Keeps medical care adequate, needed for 4 and 5 stars";
    case ITEM_RECYCLING:    return "Keeps recycling adequate, needed for 4 and 5 stars";
    case ITEM_METRO:        return "Needed for 5 stars";
    case ITEM_CATHEDRAL:    return "Hosts the wedding that crowns a TOWER";
    case ITEM_HOUSEKEEPING: return "Cleans hotel rooms; a room left dirty 3 nights gets roaches";
    case ITEM_ELEVATOR_SERVICE: return "Carries housekeeping, not guests";
    default:                return NULL;
    }
}

int hints_tool(const GameSim *sim, const Tower *tower, ItemType type,
               char lines[][HINT_LEN], int max)
{
    int n = 0;
    if (type <= ITEM_NONE || type >= ITEM_TYPE_COUNT) return 0;
    char cost[32];
    money(ITEM_COST[type], cost, sizeof cost);
    PUSHF("%s - %s%s", tower_item_name(type), cost,
          (type == ITEM_FLOOR || type == ITEM_LOBBY) ? " a segment" : "");
    const char *why = hints_unavailable(sim, tower, type);
    if (why) PUSHF("%s", why);
    if (ITEM_UNDERGROUND_ONLY[type]) PUSHF("Underground only");
    else if (type == ITEM_OFFICE || type == ITEM_CONDO ||
             item_is_hotel_room(type))
        PUSHF("Above ground only");
    if (type == ITEM_METRO || type == ITEM_CATHEDRAL) PUSHF("One per tower");
    const char *rule = placement_rule(type);
    if (rule) PUSHF("%s", rule);
    const char *what = purpose(type);
    if (what) PUSHF("%s", what);
    return n;
}

/* --------------------------------------------------------------- rating */

int hints_rating(const GameSim *sim, const Tower *tower,
                 char lines[][HINT_LEN], int max)
{
    int n = 0;
    int stars = tower->star_rating;
    if (stars >= 6) {
        PUSHF("TOWER - the highest rating there is");
        return n;
    }
    PromotionFlags f;
    game_promotion_flags(sim, tower, &f);
    char have[24];
    count_str(sim->standing_population, have, sizeof have);
    PUSHF("Rating: %s   Population %s", rating_name(stars), have);
    for (int r = stars + 1; r <= 6 && n < max; r++) {
        PUSHF("%s needs:", rating_name(r));
        char need[24];
        int pop = game_star_pop_needed(r - 1);
        count_str(pop, need, sizeof need);
        PUSHF("  %s Population %s", sim->standing_population >= pop
                                     ? "\xe2\x9c\x93" : "\xc3\x97", need);
        PromoNeed needs[PROMO_NEEDS_MAX];
        int k = game_promotion_needs(sim, tower, &f, r, needs, PROMO_NEEDS_MAX);
        for (int i = 0; i < k; i++) {
            if (needs[i].timing) PUSHF("  %s", needs[i].label);
            else PUSHF("  %s %s", needs[i].met ? "\xe2\x9c\x93" : "\xc3\x97",
                       needs[i].label);
        }
    }
    return n;
}

void hints_next_rating(const GameSim *sim, const Tower *tower,
                       char *buf, int bufn)
{
    int stars = tower->star_rating;
    if (stars >= 6) { snprintf(buf, bufn, "Your tower has reached TOWER rank"); return; }
    int next = stars + 1;
    PromotionFlags f;
    game_promotion_flags(sim, tower, &f);
    int o = snprintf(buf, bufn, "%s needs: ", rating_name(next));
    int any = 0;
    int pop = game_star_pop_needed(stars);
    if (sim->standing_population < pop && o < bufn) {
        char need[24];
        count_str(pop, need, sizeof need);
        o += snprintf(buf + o, bufn - o, "population %s", need);
        any = 1;
    }
    PromoNeed needs[PROMO_NEEDS_MAX];
    int k = game_promotion_needs(sim, tower, &f, next, needs, PROMO_NEEDS_MAX);
    for (int i = 0; i < k && o < bufn; i++) {
        if (needs[i].met || needs[i].timing) continue;
        o += snprintf(buf + o, bufn - o, "%s%s", any ? "; " : "",
                      needs[i].label);
        any = 1;
    }
    if (!any) snprintf(buf, bufn, "%s: everything is in place%s",
                       rating_name(next),
                       next == 4 || next == 5 ? " (awarded on a weekday evening)" : "");
}

/* --------------------------------------------------------- tenant notes */

static int count_type(const Tower *tower, ItemType type)
{
    int c = 0;
    for (int i = 0; i < tower->tenant_count; i++)
        if (tower->tenants[i].type == type) c++;
    return c;
}

static const char *rent_name(int cls)
{
    static const char *r[] = { "High", "Average", "Low", "Very Low" };
    return (cls >= 0 && cls <= 3) ? r[cls] : "Average";
}

int hints_tenant_notes(GameSim *sim, Tower *tower, const Tenant *t,
                       char lines[][HINT_LEN], int max)
{
    int n = 0;
    if (!t || t->state == TENANT_CONSTRUCTION) return 0;
    int is_office = t->type == ITEM_OFFICE, is_condo = t->type == ITEM_CONDO;
    int is_shop = t->type == ITEM_SHOP;
    int is_food = t->type == ITEM_RESTAURANT || t->type == ITEM_FAST_FOOD;
    int is_hotel = item_is_hotel_room(t->type);
    int lettable = is_office || is_condo || is_shop;

    /* 0. Roaches end a room for good, reachable or not. */
    if (is_hotel && t->condition == ROOM_INFESTED) {
        PUSHF("Infested: it will never take guests again, and the "
              "roaches spread to the rooms beside it. Demolish it.");
        return n;
    }

    /* 1. Cut off from the lobby: nothing else matters until it's fixed. */
    if (lettable || is_food || is_hotel || t->type == ITEM_CINEMA ||
        t->type == ITEM_PARTY_HALL) {
        int dist = 0, stairs = 0;
        int kind = people_nearest_transport(&sim->people, tower,
                       floor_to_index(t->floor), t->x, &dist, &stairs);
        if (kind == 0) {
            PUSHF("Nobody can get here from the lobby. Connect this floor "
                  "with stairs, an escalator or an elevator.");
            return n;
        }
        if (dist >= 80)
            PUSHF("The nearest %s is a long walk. Build one closer to "
                  "this unit.", kind == 2 ? "elevator"
                                          : stairs ? "staircase" : "escalator");
    }

    /* 2. Hotel rooms: a dirty room needs a housekeeper who can reach it. */
    if (is_hotel) {
        if (t->condition == ROOM_DIRTY) {
            if (count_type(tower, ITEM_HOUSEKEEPING) == 0)
                PUSHF("Dirty, and there is no Housekeeping. Build one, or "
                      "after 3 nights dirty and empty it gets roaches.");
            else
                PUSHF("Dirty: it needs a housekeeper. They ride stairs and "
                      "service elevators only, so make sure one reaches "
                      "this floor.");
        }
    }

    /* 3. Office/condo: the daily judge's verdict and its levers. */
    if (is_office || is_condo || is_hotel) {
        int metric = game_tenant_eval_metric(sim, tower, t);
        int bar = game_judge_bar(tower);
        int stress = t->pool_stress_trips
                         ? t->pool_stress_total / t->pool_stress_trips : 0;
        ItemType noisy = game_noisy_neighbor(tower, t);
        PUSHF("Opinion %d: under %d is good, %d or more is bad. Lower is "
              "better.", metric, GAME_JUDGE_HAPPY, bar);
        if (t->rent_class == 3) {
            PUSHF("Very Low rent: tenants are always satisfied here, "
                  "whatever the trip is like.");
        } else if (metric >= GAME_JUDGE_HAPPY) {
            /* the parts, biggest lever first */
            int noise = noisy != ITEM_NONE ? 60 : 0;
            int rent = t->rent_class == 0 ? 30 : t->rent_class == 2 ? -30 : 0;
            int order[3] = { 0, 1, 2 };           /* travel, noise, rent */
            int val[3] = { stress, noise, rent };
            for (int a = 0; a < 3; a++)
                for (int b = a + 1; b < 3; b++)
                    if (val[order[b]] > val[order[a]]) {
                        int tmp = order[a]; order[a] = order[b]; order[b] = tmp;
                    }
            for (int i = 0; i < 3; i++) {
                int which = order[i];
                if (val[which] <= 0) continue;
                if (which == 0)
                    PUSHF("Travel %d: elevator waits are slow. Add cars, add "
                          "a shaft nearby, or set an express schedule.",
                          stress);
                else if (which == 1)
                    PUSHF("Noise +60: the %s next door. Move one of them "
                          "away.", tower_item_name(noisy));
                else
                    PUSHF("Rent +30: %s rent. Dropping it a tier takes 30 "
                          "off; Very Low always satisfies.",
                          rent_name(t->rent_class));
            }
        }
        if (!is_hotel && t->demand_category == 0) {
            if (t->state == TENANT_EMPTY || t->state == TENANT_ABANDONED)
                PUSHF("Judged bad, so nobody will move in until it "
                      "improves.");
            else
                PUSHF("Judged bad: the tenants will move out. Fix it before "
                      "the next morning's judgement.");
        } else if (is_hotel && t->demand_category == 0) {
            PUSHF("Judged bad: the room is not sold tonight.");
        }
    }

    /* 4. Shops: judged on custom, not on the trip. */
    if (is_shop) {
        static const int adj[4] = { 5, 0, -5, -12 };
        int a = adj[t->rent_class <= 3 ? t->rent_class : 1];
        int demand = t->retail_quota + t->customers_today;
        PUSHF("Shops are judged on custom: %d today, wants %d to stay and "
              "%d to thrive. Lower rent lowers the bar.",
              demand, 20 + a, 25 + a);
        if (t->demand_category == 0)
            PUSHF("Judged bad: it will close. Very Low rent is a shop's "
                  "rescue - it takes 12 off the bar.");
    }

    /* 5. Waiting to let. */
    if (lettable && (t->state == TENANT_EMPTY ||
                     t->state == TENANT_ABANDONED) && t->demand_category != 0)
        PUSHF("Waiting for a tenant. Someone new looks each morning.");

    /* 6. The star ladder's own services. */
    if (t->type == ITEM_MEDICAL && !sim->medical_adequate)
        PUSHF("Not keeping up: a sick worker found no center in their "
              "15-floor zone or the bottom one. Build another higher up.");
    if (t->type == ITEM_RECYCLING && !sim->recycling_adequate)
        PUSHF("Not keeping up with the population: build another center "
              "next to this one.");
    return n;
}
