/* Project-authored adapters for the PC float-matrix ABI. */
#include <ultra64.h>
#include <math.h>
#include <string.h>
#include "sr64_matrix_math.h"

void guMtxF2L(float source[4][4], Mtx *destination) {
    memcpy(destination->m, source, sizeof(destination->m));
}
void guMtxIdentF(float m[4][4]) { sr64_matrix_identity(m); }
void guMtxIdent(Mtx *m) { sr64_matrix_identity(m->m); }
void guNormalize(float *x, float *y, float *z) {
    const float input[3] = {*x,*y,*z}; float output[3];
    if (sr64_vector_normalize(output,input)) { *x=output[0]; *y=output[1]; *z=output[2]; }
}
void guTranslateF(float m[4][4],float x,float y,float z) {
    const float value[3]={x,y,z};
    if (!sr64_matrix_translation(m,value)) sr64_matrix_identity(m);
}
void guTranslate(Mtx *m,float x,float y,float z) { guTranslateF(m->m,x,y,z); }
void guScaleF(float m[4][4],float x,float y,float z) {
    const float value[3]={x,y,z};
    if (!sr64_matrix_scale(m,value)) sr64_matrix_identity(m);
}
void guScale(Mtx *m,float x,float y,float z) { guScaleF(m->m,x,y,z); }
void guRotateF(float m[4][4],float degrees,float x,float y,float z) {
    const float axis[3]={x,y,z};
    if (!sr64_matrix_axis_angle(m,axis,degrees)) sr64_matrix_identity(m);
}
void guRotate(Mtx *m,float degrees,float x,float y,float z) { guRotateF(m->m,degrees,x,y,z); }
void guOrthoF(float m[4][4],float left,float right,float bottom,float top,float near,float far,float scale) {
    if (!sr64_matrix_orthographic(m,left,right,bottom,top,near,far,scale)) sr64_matrix_identity(m);
}
void guOrtho(Mtx *m,float left,float right,float bottom,float top,float near,float far,float scale) {
    guOrthoF(m->m,left,right,bottom,top,near,far,scale);
}
void guPerspectiveF(float m[4][4],u16 *norm,float fovy,float aspect,float near,float far,float scale) {
    if (!sr64_matrix_perspective(m,fovy,aspect,near,far,scale)) sr64_matrix_identity(m);
    if (norm) {
        float range=near+far;
        *norm=range>2 ? (u16)fmax(1.0,fmin(65535.0,131072.0/range)) : 65535;
    }
}
void guPerspective(Mtx *m,u16 *norm,float fovy,float aspect,float near,float far,float scale) {
    guPerspectiveF(m->m,norm,fovy,aspect,near,far,scale);
}
