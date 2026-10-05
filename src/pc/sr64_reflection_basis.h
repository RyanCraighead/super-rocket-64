#ifndef SR64_REFLECTION_BASIS_H
#define SR64_REFLECTION_BASIS_H

/* Project-authored standard vector/matrix math; no third-party code copied. */
int sr64_reflection_basis(float matrix[4][4],
                          signed char directions[2][3],
                          const float eye[3],
                          const float target[3],
                          const float up[3]);

#endif
