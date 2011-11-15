#ifndef __FIMD_API_H__
#define __FIMD_API_H__

#include <linux/fb.h>
#include "s5p_tvout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOTAL_FB_NUM		5


int fb_open(int win);
int fb_close(int fp);
int fb_on(int fp);
int fb_off(int fp);
int fb_off_all(void);
char *fb_init_display(int fp, int width, int height,\
			int left_x, int top_y, int bpp);
int fb_ioctl(int fp, __u32 cmd, void *arg);
char *fb_mmap(__u32 size, int fp);
int simple_draw(char *dest, const char *src,\
		int img_width, struct fb_var_screeninfo *var);
int draw(char *dest, const char *src,\
	int img_width, struct fb_var_screeninfo *var);
int get_fscreeninfo(int fp, struct fb_fix_screeninfo *fix);
int get_vscreeninfo(int fp, struct fb_var_screeninfo *var);
int put_vscreeninfo(int fp, struct fb_var_screeninfo *var);
int get_bytes_per_pixel(int bits_per_pixel);

#ifdef __cplusplus
}
#endif

#endif /* __FIMD_API_H__ */
