#include "thps_rails.h"
#include "level_table.h"

#include <math.h>
#include <string.h>

/* Collision vertex pairs 198/199 and 203/226, also exact visible mesh edges.
 * Provenance and a private-ROM verifier are in codex/thps/native/rails.md.
 * Flat-topped stone parapets are tagged as grindable ledges by this integration.
 * No rail meshes or arbitrary world-edge harvesting are introduced.
 */
static const ThpsHostRail sCastleRails[] = {
    { 1u, {451.0f, 964.0f, -258.0f}, {451.0f, 957.0f, -2127.0f},
      {0.0f, 0.999992986369008f, -0.003745292083779f}, 0u, 0u,
      "Castle bridge east parapet inner edge" },
    { 2u, {-450.0f, 964.0f, -264.0f}, {-450.0f, 957.0f, -2127.0f},
      {0.0f, 0.999992941120372f, -0.003757354046078f}, 0u, 0u,
      "Castle bridge west parapet inner edge" }
};

const ThpsHostRail *thps_rails_for_level(int level, int area, size_t *count) {
    int supported = level == LEVEL_CASTLE_GROUNDS && area == 1;
    if (count) *count = supported ? sizeof(sCastleRails) / sizeof(sCastleRails[0]) : 0;
    return supported ? sCastleRails : NULL;
}

const ThpsHostRail *thps_rail_by_id(int level, int area, uint32_t id) {
    size_t count, i;
    const ThpsHostRail *rails = thps_rails_for_level(level, area, &count);
    for (i = 0; i < count; ++i) if (rails[i].id == id) return &rails[i];
    return NULL;
}

int thps_rails_collision_matches(int level, int area,
                                const int16_t *collision, size_t word_count) {
    uint32_t hash = UINT32_C(2166136261);
    size_t i;
    if (!collision || word_count != THPS_CASTLE_RAIL_COLLISION_WORDS ||
        !thps_rails_for_level(level, area, NULL)) return 0;
    for (i = 0; i < word_count; ++i) {
        uint16_t word = (uint16_t)collision[i];
        hash = (hash ^ (word >> 8)) * UINT32_C(16777619);
        hash = (hash ^ (word & 255u)) * UINT32_C(16777619);
    }
    return hash == UINT32_C(0xc5945dd2);
}

static double dot(const double a[3], const double b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void cross(const double a[3], const double b[3], double out[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

static double clamp_unit(double value) {
    return value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
}

static int valid_vector(const float vector[3]) {
    int k;
    for (k = 0; k < 3; ++k)
        if (!isfinite(vector[k]) || fabsf(vector[k]) > 1e7f) return 0;
    return 1;
}

/* Closest points on two closed segments, computed in double to keep long
 * nearly parallel bridge/motion segments stable. Rail is always nondegenerate.
 */
static double closest_pair(const ThpsHostRail *rail, const ThpsHostRailQuery *query,
                           double *sweep_fraction, double *rail_fraction) {
    double motion[3], edge[3], separation[3], distance[3], perpendicular[3], numerator[3];
    double aa, bb, ab, ar, br, determinant, s, t;
    int k;
    for (k = 0; k < 3; ++k) {
        motion[k] = (double)query->current[k] - query->previous[k];
        edge[k] = (double)rail->end[k] - rail->start[k];
        separation[k] = (double)query->previous[k] - rail->start[k];
    }
    aa = dot(motion, motion); bb = dot(edge, edge);
    ab = dot(motion, edge); ar = dot(motion, separation); br = dot(edge, separation);
    if (aa <= 1e-20) { s = 0.0; t = clamp_unit(br / bb); }
    else {
        /* The equivalent aa*bb-ab*ab loses the small positive determinant
         * for very long, almost-parallel sweeps. Cross products avoid that
         * subtraction and do not turn a real crossing into a parallel miss. */
        cross(motion, edge, perpendicular);
        cross(edge, separation, numerator);
        determinant = dot(perpendicular, perpendicular);
        s = determinant > 0.0 ? clamp_unit(dot(perpendicular, numerator) / determinant) : 0.0;
        t = (ab*s + br) / bb;
        if (t < 0.0) { t = 0.0; s = clamp_unit(-ar / aa); }
        else if (t > 1.0) { t = 1.0; s = clamp_unit((ab - ar) / aa); }
    }
    for (k = 0; k < 3; ++k) distance[k] = separation[k] + motion[k]*s - edge[k]*t;
    *sweep_fraction = s; *rail_fraction = t;
    return dot(distance, distance);
}

int thps_rails_trace(int level, int area, const ThpsHostRailQuery *query,
                     ThpsHostRailHit *hit) {
    const ThpsHostRail *rails;
    ThpsHostRailHit best;
    double best_distance2, motion[3], motion_length2;
    size_t count, i;
    int k, found = 0;
    if (!query || !hit || !query->requested ||
        !valid_vector(query->previous) || !valid_vector(query->current) ||
        !valid_vector(query->motion) || !isfinite(query->radius) || query->radius < 0.0f ||
        query->radius > 1e7f || !isfinite(query->minimum_alignment) ||
        query->minimum_alignment < 0.0f || query->minimum_alignment > 1.0f) return 0;
    rails = thps_rails_for_level(level, area, &count);
    for (k = 0; k < 3; ++k) motion[k] = query->motion[k];
    motion_length2 = dot(motion, motion);
    best_distance2 = (double)query->radius * query->radius;
    memset(&best, 0, sizeof(best));
    for (i = 0; i < count; ++i) {
        const ThpsHostRail *rail = &rails[i];
        double edge[3], length, along, alignment, s, t, distance2;
        if (rail->id == query->excluded_id) continue;
        for (k = 0; k < 3; ++k) edge[k] = (double)rail->end[k] - rail->start[k];
        length = sqrt(dot(edge, edge)); along = dot(motion, edge);
        alignment = motion_length2 > 1e-20 ? fabs(along) / (length * sqrt(motion_length2)) : 0.0;
        if (alignment < query->minimum_alignment) continue;
        distance2 = closest_pair(rail, query, &s, &t);
        if (distance2 > best_distance2 || (found && distance2 == best_distance2 &&
                                          rail->id >= best.rail->id)) continue;
        best.rail = rail; best.direction = along >= 0.0 ? 1 : -1;
        best.rail_fraction = (float)t; best.sweep_fraction = (float)s;
        best.distance = (float)sqrt(distance2);
        for (k = 0; k < 3; ++k) {
            best.point[k] = (float)(rail->start[k] + edge[k]*t);
            best.sweep_point[k] = (float)(query->previous[k] +
                ((double)query->current[k] - query->previous[k])*s);
            best.tangent[k] = (float)(edge[k] / length * best.direction);
        }
        best_distance2 = distance2; found = 1;
    }
    if (found) *hit = best;
    return found;
}
