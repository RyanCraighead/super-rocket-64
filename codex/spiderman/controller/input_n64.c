#include "input_n64.h"
static float normalized(int value,int limit,float divisor){
    if(value<0){if(value< -limit)value=-limit;else if(value>=-6)value=-7;value+=7;}
    else{if(value>limit)value=limit;else if(value<7)value=7;value-=7;}
    volatile float result=(float)value/divisor;return result;
}
void smn64_input_axes(int8_t raw_x,int8_t raw_y,int8_t out[2]){
    volatile float x=normalized(raw_x,68,61.0f),y=normalized(raw_y,70,63.0f);
    volatile float lx=-y*127.0f,ly=x*127.0f;
    out[0]=(int8_t)(int32_t)lx;out[1]=(int8_t)(int32_t)ly;
}
