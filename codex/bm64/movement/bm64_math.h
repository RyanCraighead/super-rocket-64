#ifndef CODEX_BM64_MATH_H
#define CODEX_BM64_MATH_H
/* Recovered original US1.0 degree wrappers + polynomial sine/cosine.
 * Compile -ffp-contract=off and without fast-math. These deliberately preserve
 * original approximations and large-argument behavior rather than host sinf.
 */
float bm64_sin_degrees(float degrees);
float bm64_cos_degrees(float degrees);
#endif
