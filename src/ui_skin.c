/* ui_skin.c — the optional "Modern UI" skin. See ui_skin.h. */
#include "ui_skin.h"
#include <math.h>

const UiPalette UI = {
    .panel       = {  18,  22,  30, 214 },
    .header      = {  30,  36,  48, 232 },
    .raised      = {  44,  50,  64, 255 },
    .raised_hot  = {  62,  70,  88, 255 },
    .field       = {  12,  15,  21, 255 },
    .border      = { 255, 255, 255,  36 },
    .highlight   = { 255, 255, 255,  26 },
    .text        = { 232, 236, 242, 255 },
    .text_dim    = { 150, 158, 172, 255 },
    .text_off    = {  98, 106, 120, 255 },
    .accent      = {  79, 163, 255, 255 },
    .accent_soft = {  40,  86, 150, 255 },
    .gold        = { 255, 201,  64, 255 },
    .good        = {  64, 200, 230, 255 },
    .ok          = { 230, 200,  60, 255 },
    .bad         = { 235,  80,  80, 255 },
};

static SDL_Renderer *R;
static int modern;
static int chrome;          /* inside the UI pass */
static int suspended;       /* nesting depth of ui_remap_suspend */

#define MAX_HEADERS 32
static SDL_Rect headers[2][MAX_HEADERS];
static int nheaders[2];
static int cur;             /* which headers[] is this frame's */

void ui_skin_init(SDL_Renderer *r) { R = r; }
void ui_skin_set(int on)          { modern = on ? 1 : 0; }
int  ui_skin_modern(void)         { return modern; }

void ui_frame_begin(void)
{
    cur ^= 1;
    nheaders[cur] = 0;
}

void ui_chrome_begin(void) { chrome = 1; suspended = 0; }
void ui_chrome_end(void)   { chrome = 0; }
void ui_remap_suspend(void) { suspended++; }
void ui_remap_resume(void)  { if (suspended > 0) suspended--; }

static int remap_on(void) { return modern && chrome && !suspended; }
int ui_remap_active(void) { return remap_on(); }

static int is_grey(Uint8 r, Uint8 g, Uint8 b)
{
    int hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return hi - lo < 24;
}

static int luma(Uint8 r, Uint8 g, Uint8 b)
{
    return (r * 299 + g * 587 + b * 114) / 1000;
}

void ui_remap_fill(Uint8 *r, Uint8 *g, Uint8 *b, Uint8 a)
{
    if (!remap_on() || a != 255) return;   /* shadows and dims stay black */
    if (*r == 0 && *g == 0 && *b == 128) {  /* Win3.1 navy selection */
        *r = UI.accent_soft.r; *g = UI.accent_soft.g; *b = UI.accent_soft.b;
        return;
    }
    if (!is_grey(*r, *g, *b)) return;
    int l = luma(*r, *g, *b);
    SDL_Color c;
    if (l >= 235)      c = UI.field;                       /* white wells */
    else if (l >= 180) c = UI.raised;                      /* face grey */
    else if (l >= 140) c = (SDL_Color){ 60, 67, 82, 255 };
    else if (l >= 100) c = (SDL_Color){ 84, 92, 110, 255 }; /* bevel shadow */
    else if (l >= 50)  c = (SDL_Color){ 110, 118, 136, 255 };
    else               c = (SDL_Color){ 200, 206, 216, 255 }; /* black ink:
                          glyphs and rules — the outlines it also drew are
                          the primitives' job in the modern skin */
    *r = c.r; *g = c.g; *b = c.b;
}

SDL_Color ui_ink(SDL_Color c)
{
    if (!remap_on()) return c;
    int l = luma(c.r, c.g, c.b);
    if (is_grey(c.r, c.g, c.b)) {
        if (l < 110) return (SDL_Color){ UI.text.r, UI.text.g, UI.text.b, c.a };
        if (l < 200) return (SDL_Color){ UI.text_dim.r, UI.text_dim.g,
                                         UI.text_dim.b, c.a };
        return c;
    }
    if (l < 90) {             /* dark saturated ink: lift it off the glass */
        c.r = (Uint8)(c.r + (255 - c.r) * 55 / 100);
        c.g = (Uint8)(c.g + (255 - c.g) * 55 / 100);
        c.b = (Uint8)(c.b + (255 - c.b) * 55 / 100);
    }
    return c;
}

/* ---- rounded geometry ---- */

#define ARC_SEG 6
#define MAX_PTS (4 * (ARC_SEG + 1))

/* Perimeter of a rounded rect, clockwise from the top-left. */
static int round_points(float x, float y, float w, float h, int rad,
                        int corners, SDL_FPoint *p)
{
    float rmax = (w < h ? w : h) / 2.0f;
    float rr = rad > rmax ? rmax : (float)rad;
    struct { float cx, cy, a0; int bit; float px, py; } k[4] = {
        { x + rr,     y + rr,     (float)M_PI,        UI_TL, x,     y     },
        { x + w - rr, y + rr,     (float)M_PI * 1.5f, UI_TR, x + w, y     },
        { x + w - rr, y + h - rr, 0.0f,               UI_BR, x + w, y + h },
        { x + rr,     y + h - rr, (float)M_PI * 0.5f, UI_BL, x,     y + h },
    };
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (!(corners & k[i].bit) || rr < 1.0f) {
            p[n++] = (SDL_FPoint){ k[i].px, k[i].py };
            continue;
        }
        for (int s = 0; s <= ARC_SEG; s++) {
            float a = k[i].a0 + (float)M_PI * 0.5f * s / ARC_SEG;
            p[n++] = (SDL_FPoint){ k[i].cx + cosf(a) * rr,
                                   k[i].cy + sinf(a) * rr };
        }
    }
    return n;
}

void ui_fill_round(SDL_Rect r, int rad, int corners, SDL_Color c)
{
    if (!R || r.w <= 0 || r.h <= 0) return;
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_FPoint p[MAX_PTS];
    int n = round_points((float)r.x, (float)r.y, (float)r.w, (float)r.h,
                         rad, corners, p);
    SDL_Vertex v[MAX_PTS + 1];
    int idx[MAX_PTS * 3];
    v[0].position = (SDL_FPoint){ r.x + r.w / 2.0f, r.y + r.h / 2.0f };
    v[0].color = c;
    v[0].tex_coord = (SDL_FPoint){ 0, 0 };
    for (int i = 0; i < n; i++) {
        v[i + 1].position = p[i];
        v[i + 1].color = c;
        v[i + 1].tex_coord = (SDL_FPoint){ 0, 0 };
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1) % n + 1;
    }
    SDL_RenderGeometry(R, NULL, v, n + 1, idx, n * 3);
#else
    (void)rad; (void)corners;
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(R, &r);
#endif
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
}

void ui_stroke_round(SDL_Rect r, int rad, int corners, SDL_Color c)
{
    if (!R || r.w <= 1 || r.h <= 1) return;
    SDL_FPoint p[MAX_PTS + 1];
    int n = round_points(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f,
                         rad, corners, p);
    p[n] = p[0];
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLinesF(R, p, n + 1);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
}

void ui_shadow(SDL_Rect r, int rad)
{
    for (int i = 4; i >= 1; i--) {
        SDL_Rect s = { r.x - i, r.y - i + 3, r.w + 2 * i, r.h + 2 * i };
        ui_fill_round(s, rad + i, UI_ALL, (SDL_Color){ 0, 0, 0, 16 });
    }
}

void ui_panel(SDL_Rect r, int rad, int corners)
{
    ui_shadow(r, rad);
    ui_fill_round(r, rad, corners, UI.panel);
    ui_stroke_round(r, rad, corners, UI.border);
}

void ui_button(SDL_Rect r, int state)
{
    int rad = r.h < 16 ? 4 : 6;
    SDL_Color f = state == UI_BTN_HOT    ? UI.raised_hot
                : state == UI_BTN_ON     ? UI.accent_soft
                : state == UI_BTN_ACCENT ? UI.accent
                : UI.raised;
    if (state == UI_BTN_OFF) f.a = 150;
    ui_fill_round(r, rad, UI_ALL, f);
    if (state == UI_BTN_ON || state == UI_BTN_ACCENT)
        ui_stroke_round(r, rad, UI_ALL, (SDL_Color){ 120, 185, 255, 120 });
    else
        ui_stroke_round(r, rad, UI_ALL, UI.border);
    if (r.w > 2 * rad && state != UI_BTN_ON) {   /* top sheen */
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(R, 255, 255, 255, 22);
        SDL_RenderDrawLine(R, r.x + rad, r.y + 1, r.x + r.w - rad - 1, r.y + 1);
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
    }
}

void ui_field(SDL_Rect r)
{
    ui_fill_round(r, 4, UI_ALL, UI.field);
    ui_stroke_round(r, 4, UI_ALL, (SDL_Color){ 255, 255, 255, 20 });
}

void ui_close_box(SDL_Rect r)
{
    int d = r.w < r.h ? r.w : r.h;
    SDL_Rect c = { r.x + (r.w - d) / 2, r.y + (r.h - d) / 2, d, d };
    ui_fill_round(c, d / 2, UI_ALL, (SDL_Color){ 255, 255, 255, 30 });
    int m = d / 4 + 1;
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(R, UI.text.r, UI.text.g, UI.text.b, 220);
    SDL_RenderDrawLine(R, c.x + m, c.y + m, c.x + d - m - 1, c.y + d - m - 1);
    SDL_RenderDrawLine(R, c.x + d - m - 1, c.y + m, c.x + m, c.y + d - m - 1);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
}

void ui_star(int cx, int cy, int radius, int filled)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_Vertex v[11];
    int idx[30];
    SDL_Color c = filled ? UI.gold : (SDL_Color){ 255, 255, 255, 40 };
    v[0].position = (SDL_FPoint){ (float)cx, (float)cy };
    v[0].color = c;
    v[0].tex_coord = (SDL_FPoint){ 0, 0 };
    for (int i = 0; i < 10; i++) {
        float a = -(float)M_PI / 2 + (float)M_PI * i / 5;
        float rr = (i & 1) ? radius * 0.45f : (float)radius;
        v[i + 1].position = (SDL_FPoint){ cx + cosf(a) * rr, cy + sinf(a) * rr };
        v[i + 1].color = c;
        v[i + 1].tex_coord = (SDL_FPoint){ 0, 0 };
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1) % 10 + 1;
    }
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(R, NULL, v, 11, idx, 30);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
#else
    SDL_Rect r = { cx - radius / 2, cy - radius / 2, radius, radius };
    ui_fill_round(r, 2, UI_ALL, filled ? UI.gold : UI.border);
#endif
}

void ui_progress(SDL_Rect r, int permille, SDL_Color fill)
{
    int rad = r.h / 2;
    ui_fill_round(r, rad, UI_ALL, UI.field);
    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;
    int w = r.w * permille / 1000;
    if (w > 0) {
        if (w < r.h) w = r.h;
        SDL_Rect f = { r.x, r.y, w, r.h };
        ui_fill_round(f, rad, UI_ALL, fill);
    }
    ui_stroke_round(r, rad, UI_ALL, (SDL_Color){ 255, 255, 255, 24 });
}

void ui_header_note(SDL_Rect h)
{
    if (nheaders[cur] < MAX_HEADERS) headers[cur][nheaders[cur]++] = h;
}

int ui_header_above(SDL_Rect body)
{
    for (int f = 0; f < 2; f++)
        for (int i = 0; i < nheaders[f]; i++) {
            SDL_Rect h = headers[f][i];
            if (h.x == body.x && h.w == body.w && h.y + h.h == body.y)
                return 1;
        }
    return 0;
}
