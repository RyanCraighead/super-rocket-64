#ifndef SUPER_ROCKET_WHEEL_OVERLAY_H
#define SUPER_ROCKET_WHEEL_OVERLAY_H
#ifdef __cplusplus
extern "C" {
#endif
int wheel_overlay_init(void);
int wheel_overlay_render(const unsigned char *rgba,int width,int height,int framebuffer_width,int framebuffer_height);
void wheel_overlay_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
