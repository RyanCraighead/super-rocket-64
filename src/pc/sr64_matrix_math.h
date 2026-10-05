#ifndef SR64_MATRIX_MATH_H
#define SR64_MATRIX_MATH_H

/* Project-authored standard matrix/vector math; no third-party code copied. */
/* Every function returns 1 on success; on failure, its output stays untouched. */
int sr64_matrix_identity(float matrix[4][4]);
int sr64_matrix_translation(float matrix[4][4], const float translation[3]);
int sr64_matrix_scale(float matrix[4][4], const float scale[3]);
int sr64_vector_normalize(float output[3], const float input[3]);

/* Matrices use row vectors; positive Z rotation by 90 degrees maps X to Y. */
int sr64_matrix_axis_angle(float matrix[4][4],
                           const float axis[3],
                           float degrees);

/* Projection matrices use the OpenGL clip-depth range [-1, 1]. */
/* uniform_scale multiplies every matrix entry and must be finite and nonzero. */
int sr64_matrix_orthographic(float matrix[4][4],
                             float left,
                             float right,
                             float bottom,
                             float top,
                             float near_plane,
                             float far_plane,
                             float uniform_scale);
int sr64_matrix_perspective(float matrix[4][4],
                            float vertical_fov_degrees,
                            float aspect,
                            float near_plane,
                            float far_plane,
                            float uniform_scale);

#endif
