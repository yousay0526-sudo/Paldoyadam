#include "world.h"
#include <math.h>
#include <string.h>

#define V(x, y) {(x) * 1.5f, (y) * 1.5f}
/* Separate footprints preserve the gates, courtyards and lanes in the art. */
const WorldShape TRAINING_SOLIDS[] = {
    {"northwest forest", 26, {V(0, 0),     V(924, 0),   V(900, 34),  V(905, 73), V(854, 92),  V(800, 78),
                              V(760, 118), V(717, 124), V(694, 102), V(605, 95), V(572, 113), V(506, 159),
                              V(469, 195), V(432, 141), V(404, 108), V(336, 59), V(269, 50),  V(225, 52),
                              V(159, 72),  V(122, 115), V(34, 160),  V(0, 220),  V(0, 150),   V(0, 100),
                              V(0, 50),    V(0, 20)}},
    {"northeast forest",
     17,
     {V(1005, 0), V(1536, 0), V(1536, 430), V(1490, 415), V(1465, 377), V(1390, 359), V(1330, 314),
      V(1260, 292), V(1195, 279), V(1137, 245), V(1082, 227), V(1030, 168), V(1008, 124), V(980, 98),
      V(998, 70), V(1018, 38), V(1005, 15)}},
    {"central trees and rocks", 25, {V(737, 128), V(815, 126), V(868, 165),  V(894, 196),  V(932, 211),
                                     V(953, 259), V(978, 284), V(1001, 348), V(1029, 394), V(968, 421),
                                     V(881, 423), V(822, 414), V(770, 392),  V(718, 410),  V(699, 379),
                                     V(715, 337), V(699, 303), V(746, 279),  V(797, 254),  V(805, 218),
                                     V(770, 189), V(755, 177), V(742, 160),  V(737, 148),  V(735, 137)}},
    {"west forest",
     11,
     {V(0, 219), V(52, 230), V(89, 265), V(102, 327), V(92, 371), V(50, 406), V(27, 445), V(19, 510),
      V(19, 581), V(0, 650), V(0, 400)}},
    {"southwest forest",
     15,
     {V(0, 610), V(83, 654), V(180, 671), V(274, 682), V(316, 716), V(401, 766), V(448, 823), V(497, 838),
      V(569, 878), V(624, 937), V(676, 968), V(689, 1024), V(0, 1024), V(0, 850), V(0, 720)}},
    {"southern forest",
     13,
     {V(606, 940), V(695, 925), V(742, 918), V(759, 898), V(790, 903), V(816, 922), V(864, 922), V(938, 950),
      V(973, 967), V(1010, 990), V(1033, 1024), V(656, 1024), V(630, 981)}},
    {"eastern forest",
     21,
     {V(1536, 405), V(1536, 1024), V(1240, 1024), V(1180, 979), V(1168, 930), V(1128, 900), V(1136, 851),
      V(1110, 824), V(1116, 791),  V(1194, 782),  V(1230, 749), V(1280, 714), V(1280, 662), V(1309, 627),
      V(1399, 594), V(1420, 538),  V(1460, 493),  V(1471, 449), V(1490, 435), V(1506, 420), V(1520, 410)}},
    {"southwest rock island", 20, {V(394, 705), V(422, 663), V(441, 648), V(450, 614), V(489, 591),
                                   V(553, 587), V(599, 617), V(624, 620), V(654, 631), V(670, 650),
                                   V(661, 678), V(628, 699), V(591, 716), V(557, 740), V(518, 753),
                                   V(477, 756), V(437, 759), V(414, 738), V(400, 721), V(397, 712)}},
    {"smith house", 6, {V(132, 142), V(229, 119), V(285, 143), V(284, 191), V(196, 235), V(139, 214)}},
    {"smith awning", 6, {V(37, 148), V(121, 143), V(126, 211), V(106, 227), V(35, 202), V(30, 175)}},
    {"forge", 7, {V(320, 161), V(360, 152), V(392, 176), V(389, 211), V(359, 226), V(319, 209), V(307, 184)}},
    {"smith west wall",
     9,
     {V(101, 277), V(152, 280), V(208, 272), V(256, 269), V(277, 282), V(273, 299), V(222, 312), V(164, 327),
      V(113, 310)}},
    {"smith east wall",
     8,
     {V(328, 248), V(354, 238), V(416, 210), V(440, 221), V(436, 241), V(399, 259), V(344, 278),
      V(329, 269)}},
    {"smith rear fence", 4, {V(395, 120), V(425, 129), V(437, 201), V(416, 211)}},
    {"smith workbench", 4, {V(211, 226), V(246, 212), V(258, 235), V(226, 253)}},
    {"smith stool", 4, {V(262, 210), V(285, 204), V(296, 223), V(277, 237)}},
    {"herbalist house", 6, {V(534, 166), V(615, 152), V(667, 178), V(660, 223), V(602, 252), V(542, 227)}},
    {"herb garden wall",
     10,
     {V(574, 268), V(624, 250), V(656, 223), V(727, 213), V(769, 239), V(799, 276), V(777, 310), V(712, 336),
      V(650, 328), V(587, 306)}},
    {"herbalist gate pillar",
     6,
     {V(506, 238), V(522, 238), V(533, 269), V(524, 280), V(505, 270), V(501, 252)}},
    {"tavern",
     8,
     {V(36, 493), V(132, 442), V(248, 456), V(293, 474), V(293, 526), V(156, 591), V(46, 582), V(29, 548)}},
    {"tavern awning", 6, {V(293, 485), V(377, 489), V(396, 526), V(376, 550), V(329, 555), V(297, 534)}},
    {"tavern south wall",
     9,
     {V(16, 590), V(101, 612), V(188, 633), V(263, 617), V(283, 632), V(225, 650), V(135, 651), V(70, 634),
      V(8, 612)}},
    {"tavern east wall",
     8,
     {V(361, 571), V(391, 557), V(432, 532), V(434, 558), V(403, 586), V(380, 609), V(361, 613),
      V(355, 596)}},
    {"tavern north fence",
     12,
     {V(107, 349), V(200, 378), V(285, 410), V(362, 438), V(395, 458), V(393, 475), V(344, 455), V(266, 426),
      V(187, 399), V(106, 370), V(102, 361), V(103, 354)}},
    {"tavern gatepost", 4, {V(412, 483), V(430, 479), V(438, 526), V(417, 534)}},
    {"tavern table", 4, {V(200, 574), V(249, 582), V(249, 606), V(204, 599)}},
    {"tavern bench", 4, {V(304, 547), V(352, 540), V(367, 560), V(318, 576)}},
    {"village well",
     10,
     {V(502, 449), V(546, 432), V(586, 432), V(645, 445), V(655, 471), V(613, 501), V(557, 517), V(508, 498),
      V(488, 483), V(483, 462)}},
    {"small shrine", 6, {V(380, 341), V(398, 328), V(419, 346), V(420, 374), V(397, 385), V(372, 367)}},
    {"lantern base", 5, {V(445, 376), V(465, 371), V(481, 386), V(468, 401), V(435, 397)}},
    {"broken handcart", 5, {V(309, 338), V(327, 329), V(345, 345), V(339, 366), V(316, 365)}},
    {"overturned cart",
     9,
     {V(757, 479), V(800, 464), V(851, 481), V(882, 510), V(903, 526), V(894, 543), V(824, 542), V(781, 514),
      V(753, 507)}},
    {"field trees and stones",
     16,
     {V(1040, 555), V(1093, 553), V(1150, 574), V(1177, 573), V(1217, 565), V(1260, 563), V(1316, 582),
      V(1321, 607), V(1263, 622), V(1220, 617), V(1182, 601), V(1128, 610), V(1088, 590), V(1045, 588),
      V(1033, 575), V(1036, 563)}},
    {"eastern fence",
     7,
     {V(1370, 484), V(1401, 483), V(1428, 474), V(1446, 499), V(1424, 522), V(1390, 527), V(1359, 515)}},
    {"slaughter rack",
     6,
     {V(1349, 298), V(1429, 313), V(1483, 344), V(1494, 373), V(1435, 380), V(1350, 340)}},
    {"slaughter stump",
     6,
     {V(1297, 284), V(1325, 280), V(1338, 296), V(1328, 317), V(1306, 319), V(1290, 301)}}};
const int TRAINING_SOLID_COUNT = sizeof TRAINING_SOLIDS / sizeof *TRAINING_SOLIDS;
/* Only redraw these painting fragments over an actor whose feet are behind
 * the object. The actor's screen rectangle clips every fragment, so no other
 * actor, marker or UI is accidentally covered. */
const WorldCover TRAINING_COVERS[] = {
    {{"smith roof",
      8,
      {V(111, 130), V(155, 93), V(215, 65), V(299, 105), V(315, 132), V(269, 170), V(197, 188), V(137, 163)}},
     315},
    {{"forge chimney",
      12,
      {V(317, 57), V(340, 53), V(357, 69), V(358, 122), V(375, 151), V(395, 178), V(391, 211), V(320, 212),
       V(300, 193), V(302, 161), V(318, 137), V(317, 100)}},
     331.5f},
    {{"herbalist roof",
      8,
      {V(517, 153), V(556, 127), V(601, 109), V(636, 128), V(681, 169), V(656, 186), V(603, 212),
       V(547, 186)}},
     345},
    {{"tavern roof",
      12,
      {V(15, 477), V(56, 443), V(151, 417), V(189, 391), V(246, 417), V(288, 458), V(315, 469), V(286, 492),
       V(153, 546), V(61, 519), V(30, 498), V(17, 487)}},
     856.5f},
    {{"well roof", 6, {V(537, 418), V(566, 393), V(590, 414), V(587, 441), V(566, 459), V(549, 451)}},
     715.5f},
    {{"lantern post",
      7,
      {V(442, 288), V(456, 288), V(461, 314), V(480, 316), V(480, 342), V(460, 342), V(462, 390)}},
     594},
    {{"pine branches",
      13,
      {V(463, 609), V(487, 586), V(515, 576), V(545, 588), V(570, 609), V(594, 620), V(594, 647), V(570, 641),
       V(548, 664), V(523, 654), V(501, 673), V(473, 648), V(456, 629)}},
     1038},
    {{"southern bare tree",
      12,
      {V(762, 789), V(772, 823), V(798, 801), V(787, 839), V(823, 840), V(797, 859), V(784, 902), V(768, 906),
       V(777, 854), V(751, 845), V(741, 819), V(762, 836)}},
     1365}};
const int TRAINING_COVER_COUNT = sizeof TRAINING_COVERS / sizeof *TRAINING_COVERS;

const WorldShape *WORLD_SOLIDS = TRAINING_SOLIDS;
int WORLD_SOLID_COUNT = sizeof TRAINING_SOLIDS / sizeof *TRAINING_SOLIDS;
const WorldCover *WORLD_COVERS = TRAINING_COVERS;
int WORLD_COVER_COUNT = sizeof TRAINING_COVERS / sizeof *TRAINING_COVERS;
static MapId selected = MAP_TRAINING;
MapId world_selected(void) { return selected; }

typedef struct {
    float left, top, right, bottom;
} Bounds;
static Bounds bounds[64];
static bool initialized;
void world_select(MapId map) {
    if (map < 0 || map >= MAP_COUNT) map = MAP_VILLAGE;
    if (map == selected) return;
    selected = map;
    if (map == MAP_TRAINING) {
        WORLD_SOLIDS = TRAINING_SOLIDS;
        WORLD_SOLID_COUNT = TRAINING_SOLID_COUNT;
        WORLD_COVERS = TRAINING_COVERS;
        WORLD_COVER_COUNT = TRAINING_COVER_COUNT;
    } else
        world_map_data(map, &WORLD_SOLIDS, &WORLD_SOLID_COUNT, &WORLD_COVERS, &WORLD_COVER_COUNT);
    initialized = false;
}
static void init_bounds(void) {
    if (initialized)
        return;
    for (int i = 0; i < WORLD_SOLID_COUNT; i++) {
        const WorldShape *s = &WORLD_SOLIDS[i];
        Bounds b = {s->points[0].x, s->points[0].y, s->points[0].x, s->points[0].y};
        for (int j = 1; j < s->count; j++) {
            b.left = fminf(b.left, s->points[j].x);
            b.right = fmaxf(b.right, s->points[j].x);
            b.top = fminf(b.top, s->points[j].y);
            b.bottom = fmaxf(b.bottom, s->points[j].y);
        }
        bounds[i] = b;
    }
    initialized = true;
}
static float edge_distance(Vec2 p, Vec2 a, Vec2 b) {
    float x = b.x - a.x, y = b.y - a.y, n = x * x + y * y;
    float t = n > 0 ? clampf(((p.x - a.x) * x + (p.y - a.y) * y) / n, 0, 1) : 0;
    return hypotf(p.x - a.x - t * x, p.y - a.y - t * y);
}
static bool contains(const WorldShape *s, Vec2 p) {
    bool in = false;
    for (int i = 0, j = s->count - 1; i < s->count; j = i++) {
        Vec2 a = s->points[i], b = s->points[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
            in = !in;
    }
    return in;
}
bool world_clear(Vec2 p, float r) {
    if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(r) || r < 0 || p.x < 28 || p.y < 28 ||
        p.x > WORLD_W - 28 || p.y > WORLD_H - 28)
        return false;
    init_bounds();
    for (int i = 0; i < WORLD_SOLID_COUNT; i++) {
        Bounds b = bounds[i];
        if (p.x < b.left - r || p.x > b.right + r || p.y < b.top - r || p.y > b.bottom + r)
            continue;
        const WorldShape *s = &WORLD_SOLIDS[i];
        if (contains(s, p))
            return false;
        for (int j = 0; j < s->count; j++)
            if (edge_distance(p, s->points[j], s->points[(j + 1) % s->count]) <= r + .01f)
                return false;
    }
    return true;
}
static float cross(Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
static bool crossing(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    if (fmaxf(a.x, b.x) < fminf(c.x, d.x) || fmaxf(c.x, d.x) < fminf(a.x, b.x) ||
        fmaxf(a.y, b.y) < fminf(c.y, d.y) || fmaxf(c.y, d.y) < fminf(a.y, b.y))
        return false;
    return cross(a, b, c) * cross(a, b, d) <= 0 && cross(c, d, a) * cross(c, d, b) <= 0;
}
bool world_segment(Vec2 a, Vec2 b, float r) {
    if (!world_clear(a, r) || !world_clear(b, r))
        return false;
    for (int i = 0; i < WORLD_SOLID_COUNT; i++) {
        Bounds box = bounds[i];
        if (fmaxf(a.x, b.x) + r < box.left || fminf(a.x, b.x) - r > box.right ||
            fmaxf(a.y, b.y) + r < box.top || fminf(a.y, b.y) - r > box.bottom)
            continue;
        const WorldShape *s = &WORLD_SOLIDS[i];
        for (int j = 0; j < s->count; j++) {
            Vec2 c = s->points[j], d = s->points[(j + 1) % s->count];
            if (crossing(a, b, c, d) || edge_distance(c, a, b) <= r || edge_distance(d, a, b) <= r ||
                edge_distance(a, c, d) <= r || edge_distance(b, c, d) <= r)
                return false;
        }
    }
    return true;
}
/* Shortest share of a step, along the wall, that still slides (sin 20°). */
#define WORLD_SLIDE_MIN .34f
/* Outward normal of the nearest solid edge within reach of p, if any. */
static bool contact_normal(Vec2 p, float reach, Vec2 *normal) {
    float best = reach;
    bool found = false;
    for (int i = 0; i < WORLD_SOLID_COUNT; i++) {
        Bounds b = bounds[i];
        if (p.x < b.left - reach || p.x > b.right + reach || p.y < b.top - reach || p.y > b.bottom + reach)
            continue;
        const WorldShape *s = &WORLD_SOLIDS[i];
        for (int j = 0; j < s->count; j++) {
            Vec2 a = s->points[j], c = s->points[(j + 1) % s->count];
            float x = c.x - a.x, y = c.y - a.y, n = x * x + y * y;
            float t = n > 0 ? clampf(((p.x - a.x) * x + (p.y - a.y) * y) / n, 0, 1) : 0;
            float dx = p.x - a.x - t * x, dy = p.y - a.y - t * y, d = hypotf(dx, dy);
            if (d < best && d > .0001f) {
                best = d;
                *normal = (Vec2){dx / d, dy / d};
                found = true;
            }
        }
    }
    return found;
}
Vec2 world_move(Vec2 p, Vec2 delta, float radius) {
    if (!isfinite(delta.x) || !isfinite(delta.y) || !world_clear(p, radius))
        return p;
    float length = hypotf(delta.x, delta.y);
    if (length > WORLD_W + WORLD_H)
        return p;
    int steps = (int)ceilf(length / 3.0f);
    if (steps < 1)
        return p;
    delta.x /= steps;
    delta.y /= steps;
    for (int i = 0; i < steps; i++) {
        Vec2 q = {clampf(p.x + delta.x, 28, WORLD_W - 28), clampf(p.y + delta.y, 28, WORLD_H - 28)};
        if (world_segment(p, q, radius)) {
            p = q;
            continue;
        }
        /* Collide and slide: drop the part of the step that pushes into the
         * nearest wall and keep the part along it, at any wall angle. Pushing
         * almost straight into a wall stays put instead of drifting. */
        Vec2 n = {0, 0};
        if (contact_normal(p, radius + 4, &n)) {
            float into = delta.x * n.x + delta.y * n.y, step = hypotf(delta.x, delta.y);
            if (into < 0 && hypotf(delta.x - n.x * into, delta.y - n.y * into) >= WORLD_SLIDE_MIN * step) {
                Vec2 along = {clampf(p.x + delta.x - n.x * into, 28, WORLD_W - 28),
                              clampf(p.y + delta.y - n.y * into, 28, WORLD_H - 28)};
                if (world_segment(p, along, radius)) {
                    p = along;
                    continue;
                }
            }
        }
        /* Corners and the map border: fall back to sliding along each axis.
         * Segment tests also cover dashes and narrow walls between two legal
         * endpoints. */
        Vec2 x = {q.x, p.y}, y = {p.x, q.y};
        if (world_segment(p, x, radius))
            p = x;
        y.x = p.x;
        if (world_segment(p, y, radius))
            p = y;
    }
    return p;
}

#define CELL 24
#define COLS 96
#define ROWS 64
#define NODES (COLS * ROWS)
static const int DX[8] = {1, 0, -1, 0, 1, -1, -1, 1}, DY[8] = {0, 1, 0, -1, 1, 1, -1, -1};
typedef struct {
    bool ready;
    float radius;
    MapId map;
    unsigned char edges[NODES], clear[NODES];
} NavGrid;
static NavGrid grids[MAP_COUNT * 3];
static unsigned next_grid;
static Vec2 center(int index) {
    return (Vec2){(index % COLS + .5f) * CELL, (index / COLS + .5f) * CELL};
}
static NavGrid *grid(float radius) {
    for (int i = 0; i < MAP_COUNT * 3; i++)
        if (grids[i].ready && grids[i].map == selected && grids[i].radius == radius)
            return &grids[i];
    NavGrid *g = &grids[next_grid++ % (MAP_COUNT * 3)];
    memset(g, 0, sizeof *g);
    g->radius = radius;
    g->map = selected;
    for (int i = 0; i < NODES; i++)
        g->clear[i] = world_clear(center(i), radius);
    for (int i = 0; i < NODES; i++)
        if (g->clear[i]) {
            int x = i % COLS, y = i / COLS;
            for (int d = 0; d < 8; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS)
                    continue;
                int n = ny * COLS + nx;
                if (g->clear[n] && world_segment(center(i), center(n), radius))
                    g->edges[i] |= 1u << d;
            }
        }
    g->ready = true;
    return g;
}
void world_prepare(void) {
    grid(HERO_RADIUS);
    grid(BOAR_RADIUS);
    grid(BOSS_RADIUS);
}
bool world_route(Vec2 start, Vec2 goal, float radius, Route *out) {
    memset(out, 0, sizeof *out);
    if (!world_clear(start, radius) || !isfinite(goal.x) || !isfinite(goal.y))
        return false;
    goal.x = clampf(goal.x, 28, WORLD_W - 28);
    goal.y = clampf(goal.y, 28, WORLD_H - 28);
    if (world_segment(start, goal, radius)) {
        out->points[out->count++] = goal;
        return true;
    }
    NavGrid *g = grid(radius);
    /* BFS is cached-grid navigation, not per-frame pixel sampling. Only nodes
     * connected to the start are considered, including for blocked taps. */
    short parents[NODES], queue[NODES], chain[NODES];
    for (int i = 0; i < NODES; i++)
        parents[i] = -2;
    int seed = -1;
    float nearest = 1e9f;
    for (int i = 0; i < NODES; i++)
        if (g->clear[i]) {
            float d = distance(start, center(i));
            if (d < nearest && d < CELL * 3 && world_segment(start, center(i), radius)) {
                nearest = d;
                seed = i;
            }
        }
    if (seed < 0)
        return false;
    int head = 0, tail = 0, best = seed;
    float best_distance = distance(center(seed), goal);
    queue[tail++] = (short)seed;
    parents[seed] = -1;
    while (head < tail) {
        int n = queue[head++];
        float d = distance(center(n), goal);
        if (d < best_distance) {
            best_distance = d;
            best = n;
        }
        for (int k = 0; k < 8; k++)
            if (g->edges[n] & (1u << k)) {
                int next = n + DY[k] * COLS + DX[k];
                if (parents[next] != -2)
                    continue;
                parents[next] = (short)n;
                queue[tail++] = (short)next;
            }
    }
    if (best_distance > 180)
        return false;
    int length = 0;
    for (int n = best; n >= 0; n = parents[n])
        chain[length++] = (short)n;
    Vec2 anchor = start;
    int index = length - 1;
    while (index >= 0) {
        int next = index;
        /* Greedy line-of-sight smoothing keeps the corners clear by the same
         * foot radius used by actual movement. */
        while (next > 0 && world_segment(anchor, center(chain[next - 1]), radius))
            next--;
        Vec2 point = center(chain[next]);
        if (!world_segment(anchor, point, radius) || out->count >= ROUTE_MAX) {
            out->count = 0;
            return false;
        }
        out->points[out->count++] = point;
        anchor = point;
        index = next - 1;
    }
    if (world_segment(anchor, goal, radius) && distance(anchor, goal) > .1f) {
        if (out->count >= ROUTE_MAX) {
            out->count = 0;
            return false;
        }
        out->points[out->count++] = goal;
    }
    return out->count > 0;
}
Vec2 world_follow(Route *r, Vec2 p) {
    while (r->cursor < r->count && distance(p, r->points[r->cursor]) < 2)
        r->cursor++;
    if (r->cursor >= r->count)
        return (Vec2){0, 0};
    Vec2 q = r->points[r->cursor];
    float d = distance(p, q);
    return (Vec2){(q.x - p.x) / d, (q.y - p.y) / d};
}
