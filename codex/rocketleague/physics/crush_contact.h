#ifndef ROCKET_CRUSH_CONTACT_H
#define ROCKET_CRUSH_CONTACT_H
#include "body_contact.h"
/* Clip a ceiling triangle to the complete chassis' projected convex footprint.
 * Return the lowest overlapping ceiling height, including edge/roof contacts.
 * The host decides whether a real floor and a crushing actor trap this body. */
static inline int rocket_crush_ceiling(const RocketSnapshot *car,
        const float triangle[3][3],float *height,float *bottom) {
    if(!height||!bottom||!rocket_body_pose_valid(car))return 0;
    RocketBodyPoint points[8],hull[16];
    const float half[3]={ROCKET_BODY_HALF_LENGTH,ROCKET_BODY_HALF_WIDTH,ROCKET_BODY_HALF_HEIGHT};
    *bottom=INFINITY;
    for(int i=0;i<8;i++) {
        float v[3];
        for(int k=0;k<3;k++) {
            v[k]=car->position[k]+car->basis[k]*ROCKET_BODY_FORWARD_OFFSET+car->basis[6+k]*ROCKET_BODY_UP_OFFSET;
            for(int a=0;a<3;a++)v[k]+=car->basis[3*a+k]*half[a]*((i&(1<<a))?1.f:-1.f);
        }
        points[i]=(RocketBodyPoint){v[0],v[2]};*bottom=fminf(*bottom,v[1]);
    }
    for(int i=1;i<8;i++) {
        RocketBodyPoint v=points[i];int j=i;
        while(j>0&&(points[j-1].x>v.x||(points[j-1].x==v.x&&points[j-1].z>v.z))) {points[j]=points[j-1];j--;}
        points[j]=v;
    }
    int n=0;
    for(int i=0;i<8;i++) {while(n>=2&&rocket_body_cross(hull[n-2],hull[n-1],points[i])<=0)n--;hull[n++]=points[i];}
    int lower=n;
    for(int i=6;i>=0;i--) {while(n>lower&&rocket_body_cross(hull[n-2],hull[n-1],points[i])<=0)n--;hull[n++]=points[i];}
    if(n>1)n--;
    if(n<3)return 0;
    float polygon[16][3],next[16][3];int count=3;
    for(int i=0;i<3;i++)for(int k=0;k<3;k++) {if(!isfinite(triangle[i][k]))return 0;polygon[i][k]=triangle[i][k];}
    for(int edge=0;edge<n&&count;edge++) {
        int used=0;
        for(int i=0;i<count;i++) {
            const float *a=polygon[i],*b=polygon[(i+1)%count];
            float da=rocket_body_cross(hull[edge],hull[(edge+1)%n],(RocketBodyPoint){a[0],a[2]});
            float db=rocket_body_cross(hull[edge],hull[(edge+1)%n],(RocketBodyPoint){b[0],b[2]});
            if(da>=0) {if(used>=16)return 0;for(int k=0;k<3;k++)next[used][k]=a[k];used++;}
            if((da<0)!=(db<0)) {
                if(used>=16)return 0;
                float t=da/(da-db);for(int k=0;k<3;k++)next[used][k]=a[k]+t*(b[k]-a[k]);used++;
            }
        }
        count=used;for(int i=0;i<count;i++)for(int k=0;k<3;k++)polygon[i][k]=next[i][k];
    }
    if(count<3)return 0;
    float area=0;*height=INFINITY;
    for(int i=0;i<count;i++) {
        int j=(i+1)%count;area+=polygon[i][0]*polygon[j][2]-polygon[j][0]*polygon[i][2];
        *height=fminf(*height,polygon[i][1]);
    }
    return fabsf(area)>1.f&&isfinite(*height);
}
#endif
