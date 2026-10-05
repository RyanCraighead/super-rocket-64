#include "sr64_reflection_basis.h"

#include <float.h>
#include <limits.h>
#include <math.h>

static int finite_vec3(const double v[3])
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

static double norm3(const double v[3])
{
    return hypot(hypot(v[0], v[1]), v[2]);
}

static void cross3(const double a[3], const double b[3], double out[3])
{
    const double x = a[1] * b[2] - a[2] * b[1];
    const double y = a[2] * b[0] - a[0] * b[2];
    const double z = a[0] * b[1] - a[1] * b[0];
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

static double dot3(const double a[3], const double b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static int normalize3(const double in[3], double out[3])
{
    const double length = norm3(in);
    int i;

    if (!finite_vec3(in) || !isfinite(length) || !(length > 0.0))
        return 0;

    for (i = 0; i < 3; ++i)
        out[i] = in[i] / length;

    return finite_vec3(out);
}

static signed char direction_component(double component)
{
    const double scaled = component * 128.0;

    if (scaled >= (double)SCHAR_MAX)
        return (signed char)SCHAR_MAX;
    if (scaled <= (double)SCHAR_MIN)
        return (signed char)SCHAR_MIN;
    return (signed char)scaled;
}

int sr64_reflection_basis(float matrix[4][4],
                          signed char directions[2][3],
                          const float eye[3],
                          const float target[3],
                          const float up[3])
{
    double eye_d[3];
    double target_d[3];
    double up_d[3];
    double up_unit[3];
    double backwards[3];
    double right_raw[3];
    double right[3];
    double corrected_raw[3];
    double corrected_up[3];
    double delta[3];
    double basis[3][3];
    double matrix_d[4][4];
    float matrix_f[4][4];
    signed char directions_s[2][3];
    int row;
    int column;

    if (matrix == 0 || directions == 0 || eye == 0 || target == 0 || up == 0)
        return 0;

    if (CHAR_BIT != 8 || SCHAR_MIN != -128 || SCHAR_MAX != 127)
        return 0;

    for (row = 0; row < 3; ++row) {
        eye_d[row] = (double)eye[row];
        target_d[row] = (double)target[row];
        up_d[row] = (double)up[row];
    }
    if (!finite_vec3(eye_d) || !finite_vec3(target_d) || !finite_vec3(up_d))
        return 0;

    for (row = 0; row < 3; ++row)
        delta[row] = eye_d[row] - target_d[row];
    if (!normalize3(delta, backwards) || !normalize3(up_d, up_unit))
        return 0;

    cross3(up_unit, backwards, right_raw);
    if (!normalize3(right_raw, right))
        return 0;

    cross3(backwards, right, corrected_raw);
    if (!normalize3(corrected_raw, corrected_up))
        return 0;

    for (row = 0; row < 3; ++row) {
        basis[row][0] = right[row];
        basis[row][1] = corrected_up[row];
        basis[row][2] = backwards[row];
    }

    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column)
            matrix_d[row][column] = basis[row][column];
        matrix_d[row][3] = 0.0;
    }
    for (column = 0; column < 3; ++column) {
        const double basis_column[3] = {
            basis[0][column], basis[1][column], basis[2][column]
        };
        matrix_d[3][column] = -dot3(eye_d, basis_column);
    }
    matrix_d[3][3] = 1.0;

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
            const double value = matrix_d[row][column];
            if (!isfinite(value) || value > (double)FLT_MAX ||
                value < -(double)FLT_MAX)
                return 0;
            matrix_f[row][column] = (float)value;
            if (!isfinite((double)matrix_f[row][column]))
                return 0;
        }
    }

    for (row = 0; row < 3; ++row) {
        directions_s[0][row] = direction_component(right[row]);
        directions_s[1][row] = direction_component(corrected_up[row]);
    }

    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column)
            matrix[row][column] = matrix_f[row][column];
    for (row = 0; row < 2; ++row)
        for (column = 0; column < 3; ++column)
            directions[row][column] = directions_s[row][column];

    return 1;
}
