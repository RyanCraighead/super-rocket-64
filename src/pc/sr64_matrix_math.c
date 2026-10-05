#include "sr64_matrix_math.h"

#include <float.h>
#include <math.h>

static int finite3(const double v[3])
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

static double length3(const double v[3])
{
    return hypot(hypot(v[0], v[1]), v[2]);
}

static int normalize_double3(const double input[3], double output[3])
{
    const double length = length3(input);
    int i;

    if (!finite3(input) || !isfinite(length) || !(length > 0.0))
        return 0;
    for (i = 0; i < 3; ++i)
        output[i] = input[i] / length;
    return finite3(output);
}

static void identity_double(double matrix[4][4])
{
    int row;
    int column;
    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column)
            matrix[row][column] = row == column ? 1.0 : 0.0;
}

static int commit_matrix(float output[4][4], const double input[4][4])
{
    float staged[4][4];
    int row;
    int column;

    if (output == 0)
        return 0;
    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
            const double value = input[row][column];
            if (!isfinite(value) || value > (double)FLT_MAX ||
                value < -(double)FLT_MAX)
                return 0;
            staged[row][column] = (float)value;
            if (!isfinite((double)staged[row][column]) ||
                (value != 0.0 && staged[row][column] == 0.0f))
                return 0;
        }
    }
    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column)
            output[row][column] = staged[row][column];
    return 1;
}

int sr64_matrix_identity(float matrix[4][4])
{
    double staged[4][4];
    identity_double(staged);
    return commit_matrix(matrix, staged);
}

int sr64_matrix_translation(float matrix[4][4], const float translation[3])
{
    double staged[4][4];
    int i;

    if (matrix == 0 || translation == 0)
        return 0;
    for (i = 0; i < 3; ++i)
        if (!isfinite((double)translation[i]))
            return 0;

    identity_double(staged);
    for (i = 0; i < 3; ++i)
        staged[3][i] = (double)translation[i];
    return commit_matrix(matrix, staged);
}

int sr64_matrix_scale(float matrix[4][4], const float scale[3])
{
    double staged[4][4];
    int i;

    if (matrix == 0 || scale == 0)
        return 0;
    for (i = 0; i < 3; ++i)
        if (!isfinite((double)scale[i]))
            return 0;

    identity_double(staged);
    for (i = 0; i < 3; ++i)
        staged[i][i] = (double)scale[i];
    return commit_matrix(matrix, staged);
}

int sr64_vector_normalize(float output[3], const float input[3])
{
    double source[3];
    double normalized[3];
    float staged[3];
    int i;

    if (output == 0 || input == 0)
        return 0;
    for (i = 0; i < 3; ++i) {
        source[i] = (double)input[i];
        if (!isfinite(source[i]))
            return 0;
    }
    if (!normalize_double3(source, normalized))
        return 0;
    for (i = 0; i < 3; ++i) {
        staged[i] = (float)normalized[i];
        if (!isfinite((double)staged[i]))
            return 0;
    }
    for (i = 0; i < 3; ++i)
        output[i] = staged[i];
    return 1;
}

int sr64_matrix_axis_angle(float matrix[4][4],
                           const float axis[3],
                           float degrees)
{
    const double pi = 3.14159265358979323846264338327950288;
    double axis_d[3];
    double unit[3];
    double staged[4][4];
    double angle;
    double sine;
    double cosine;
    double one_minus_cosine;
    double x;
    double y;
    double z;
    int i;

    if (matrix == 0 || axis == 0 || !isfinite((double)degrees))
        return 0;
    for (i = 0; i < 3; ++i) {
        axis_d[i] = (double)axis[i];
        if (!isfinite(axis_d[i]))
            return 0;
    }
    if (!normalize_double3(axis_d, unit))
        return 0;

    angle = (double)degrees * pi / 180.0;
    sine = sin(angle);
    cosine = cos(angle);
    if (!isfinite(angle) || !isfinite(sine) || !isfinite(cosine))
        return 0;
    one_minus_cosine = 1.0 - cosine;
    x = unit[0];
    y = unit[1];
    z = unit[2];

    identity_double(staged);
    /* Transposed Rodrigues matrix for row-vector, right-handed rotation. */
    staged[0][0] = cosine + x * x * one_minus_cosine;
    staged[0][1] = x * y * one_minus_cosine + z * sine;
    staged[0][2] = x * z * one_minus_cosine - y * sine;
    staged[1][0] = x * y * one_minus_cosine - z * sine;
    staged[1][1] = cosine + y * y * one_minus_cosine;
    staged[1][2] = y * z * one_minus_cosine + x * sine;
    staged[2][0] = x * z * one_minus_cosine + y * sine;
    staged[2][1] = y * z * one_minus_cosine - x * sine;
    staged[2][2] = cosine + z * z * one_minus_cosine;
    return commit_matrix(matrix, staged);
}

int sr64_matrix_orthographic(float matrix[4][4],
                             float left,
                             float right,
                             float bottom,
                             float top,
                             float near_plane,
                             float far_plane,
                             float uniform_scale)
{
    double staged[4][4];
    double width;
    double height;
    double depth;
    const double values[7] = {
        (double)left, (double)right, (double)bottom, (double)top,
        (double)near_plane, (double)far_plane, (double)uniform_scale
    };
    int i;
    int row;
    int column;

    if (matrix == 0)
        return 0;
    for (i = 0; i < 7; ++i)
        if (!isfinite(values[i]))
            return 0;
    if (right == left || top == bottom || far_plane == near_plane ||
        uniform_scale == 0.0f)
        return 0;

    width = (double)right - (double)left;
    height = (double)top - (double)bottom;
    depth = (double)far_plane - (double)near_plane;
    identity_double(staged);
    staged[0][0] = 2.0 / width;
    staged[1][1] = 2.0 / height;
    staged[2][2] = -2.0 / depth;
    staged[3][0] = -((double)right + (double)left) / width;
    staged[3][1] = -((double)top + (double)bottom) / height;
    staged[3][2] = -((double)far_plane + (double)near_plane) / depth;

    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column)
            staged[row][column] *= (double)uniform_scale;
    return commit_matrix(matrix, staged);
}

int sr64_matrix_perspective(float matrix[4][4],
                            float vertical_fov_degrees,
                            float aspect,
                            float near_plane,
                            float far_plane,
                            float uniform_scale)
{
    const double pi = 3.14159265358979323846264338327950288;
    double staged[4][4] = {{0.0}};
    const double fov = (double)vertical_fov_degrees;
    const double aspect_d = (double)aspect;
    const double near_d = (double)near_plane;
    const double far_d = (double)far_plane;
    const double scale_d = (double)uniform_scale;
    const double tangent = tan(fov * pi / 360.0);
    double f;
    int row;
    int column;

    if (matrix == 0 || !isfinite(fov) || !isfinite(aspect_d) ||
        !isfinite(near_d) || !isfinite(far_d) || !isfinite(scale_d))
        return 0;
    if (!(fov > 0.0 && fov < 180.0) || !(aspect_d > 0.0) ||
        !(near_d > 0.0) || !(far_d > near_d) || scale_d == 0.0)
        return 0;
    if (!isfinite(tangent) || !(tangent > 0.0))
        return 0;

    f = 1.0 / tangent;
    staged[0][0] = f / aspect_d;
    staged[1][1] = f;
    staged[2][2] = (near_d + far_d) / (near_d - far_d);
    staged[2][3] = -1.0;
    staged[3][2] = (2.0 * near_d * far_d) / (near_d - far_d);
    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column)
            staged[row][column] *= scale_d;
    return commit_matrix(matrix, staged);
}
