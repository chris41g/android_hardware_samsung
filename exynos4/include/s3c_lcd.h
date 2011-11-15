/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _S3CFB_LCD_
#define _S3CFB_LCD_

#if 0
typedef struct {
	unsigned int *fb;
	unsigned int fb_fp;
} fb_ptr;

typedef struct {
    int bpp;
    int left_x;
    int top_y;
    int width;
    int height;
} s3cfb_win_info_t;

#else
/*
 * S T R U C T U R E S  F O R  C U S T O M  I O C T L S
 *
*/
struct s3cfb_user_window {
	int x;
	int y;
};

struct s3cfb_user_plane_alpha {
	int 		channel;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};

struct s3cfb_user_chroma {
	int 		enabled;
	unsigned char	red;
	unsigned char	green;
	unsigned char	blue;
};
#endif


typedef struct {
	unsigned int phy_start_addr;
	unsigned int xres;		/* visible resolution*/
	unsigned int yres;
	unsigned int xres_virtual;	/* virtual resolution*/
	unsigned int yres_virtual;
	unsigned int xoffset;	/* offset from virtual to visible */
	unsigned int yoffset;	/* resolution	*/
	unsigned int lcd_offset_x;
	unsigned int lcd_offset_y;
} s3c_fb_next_info_t;


#if 0
#define S3CFB_VS_START                  _IO  ('F', 103)
#define S3CFB_VS_STOP                   _IO  ('F', 104)
#define S3CFB_VS_MOVE                   _IOW ('F', 106, unsigned int)
#define S3CFB_SET_VSYNC_INT             _IOW ('F', 309, int)
#define S3CFB_OSD_ALPHA_UP	        _IO  ('F', 203)
#define S3CFB_OSD_ALPHA_DOWN	        _IO  ('F', 204)
#define S3CFB_OSD_START 	        _IO('F', 201)
#define S3CFB_OSD_STOP 		        _IO('F', 202)
#define S3CFB_OSD_SET_INFO              _IOW ('F', 209, s3cfb_win_info_t)

#define S3C_FB_OSD_ALPHA_SET	        _IOW ('F', 210, unsigned int)
#define S3C_FB_SET_NEXT_FB_INFO	        _IOW('F', 320, s3c_fb_next_info_t)
#define S3C_FB_GET_CURR_FB_INFO	        _IOR('F', 321, s3c_fb_next_info_t)

#define FBIO_WAITFORVSYNC	        _IOW ('F', 32, unsigned int)

#else

/*
 * C U S T O M  I O C T L S
 *
*/

#define FBIO_WAITFORVSYNC		_IO  ('F', 32)
#define S3CFB_WIN_POSITION		_IOW ('F', 203, struct s3cfb_user_window)
#define S3CFB_WIN_SET_PLANE_ALPHA	_IOW ('F', 204, struct s3cfb_user_plane_alpha)
#define S3CFB_WIN_SET_CHROMA		_IOW ('F', 205, struct s3cfb_user_chroma)
#define S3CFB_SET_VSYNC_INT		_IOW ('F', 206, unsigned int)
#define S3CFB_SET_SUSPEND_FIFO		_IOW ('F', 300, unsigned long)
#define S3CFB_SET_RESUME_FIFO		_IOW ('F', 301, unsigned long)
#define S3CFB_GET_LCD_WIDTH		_IOR ('F', 302, int)
#define S3CFB_GET_LCD_HEIGHT		_IOR ('F', 303, int)
#define S3CFB_GET_FB_PHY_ADDR           _IOR ('F', 310, unsigned int)
#if 1
// added by jamie (2009.08.18)
#define S3C_FB_GET_CURR_FB_INFO		_IOR ('F', 305, s3c_fb_next_info_t)
#endif
#endif

#define LCD_WIDTH       800
#define LCD_HEIGHT      480

/***************** LCD frame buffer *****************/
#define FB0_NAME    "/dev/fb0"
#define FB1_NAME    "/dev/fb1"
#define FB2_NAME    "/dev/fb2"
#define FB3_NAME    "/dev/fb3"
#define FB4_NAME    "/dev/fb4"

#if 0
int disp_win_init(int win_num, int left, int top, int width, int height, char color, fb_ptr *ptr);
int disp_win_deinit(int width, int height, fb_ptr *ptr);
void draw(char *dest, char *src, int width, int height);
#endif

#endif
