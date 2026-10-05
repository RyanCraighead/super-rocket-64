/* Super Rocket 64 adapter for project-authored reflection camera math. */
#include <ultra64.h>
#include <string.h>
#include "sr64_reflection_basis.h"

void guLookAtReflectF(float matrix[4][4], LookAt *look,
                     float ex, float ey, float ez,
                     float tx, float ty, float tz,
                     float ux, float uy, float uz) {
    const float eye[3] = {ex, ey, ez};
    const float target[3] = {tx, ty, tz};
    const float up[3] = {ux, uy, uz};
    signed char directions[2][3];
    if (!sr64_reflection_basis(matrix, directions, eye, target, up)) {
        /* A degenerate view must not introduce NaNs into render state. */
        const float origin[3] = {0, 0, 0};
        const float forward[3] = {0, 0, -1};
        const float vertical[3] = {0, 1, 0};
        sr64_reflection_basis(matrix, directions, origin, forward, vertical);
    }
    memset(look, 0, sizeof(*look));
    for (int axis = 0; axis < 3; ++axis) {
        look->l[0].l.dir[axis] = directions[0][axis];
        look->l[1].l.dir[axis] = directions[1][axis];
    }
    look->l[1].l.col[1] = 128;
    look->l[1].l.colc[1] = 128;
}

void guLookAtReflect(Mtx *matrix, LookAt *look,
                    float ex, float ey, float ez,
                    float tx, float ty, float tz,
                    float ux, float uy, float uz) {
    float real_matrix[4][4];
    guLookAtReflectF(real_matrix, look, ex, ey, ez, tx, ty, tz, ux, uy, uz);
    guMtxF2L(real_matrix, matrix);
}
