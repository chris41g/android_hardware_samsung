/*
**
** Copyright 2008, The Android Open Source Project
** Copyright@ Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ANDROID_HARDWARE_CAMERA_SEC_H
#define ANDROID_HARDWARE_CAMERA_SEC_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <linux/videodev2.h>
#include <videodev2_samsung.h>

#include <camera/CameraHardwareInterface.h>

namespace android {

//#define JPEG_FROM_SENSOR //Define this if the JPEG images are obtained directly from camera sensor. Else on chip JPEG encoder will be used.
#include "jpeg_api.h"

#define PREVIEW_USING_MMAP //Define this if the preview data is to be shared using memory mapped technique instead of passing physical address.

#define DUAL_PORT_RECORDING //Define this if 2 fimc ports are needed for recording.

//#define INCLUDE_JPEG_THUMBNAIL 0 //Valid only for on chip JPEG encoder

//#define PERFORMANCE     //Uncomment to measure performance

//#define DUMP_YUV        //Uncomment to take a dump of YUV frame during capture

//#define USE_SEC_CROP_ZOOM // use digital zoom using crop ( <-> optical zoom)


#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
#define LOG_CAMERA LOGD
#define LOG_CAMERA_PREVIEW LOGD

#define LOG_TIME_DEFINE(n) \
	struct timeval time_start_##n, time_stop_##n; unsigned long log_time_##n = 0;

#define LOG_TIME_START(n) \
	gettimeofday(&time_start_##n, NULL);

#define LOG_TIME_END(n) \
	gettimeofday(&time_stop_##n, NULL); log_time_##n = measure_time(&time_start_##n, &time_stop_##n);

#define LOG_TIME(n) \
	log_time_##n

#else
#define LOG_CAMERA(...)
#define LOG_CAMERA_PREVIEW(...)
#define LOG_TIME_DEFINE(n)
#define LOG_TIME_START(n)
#define LOG_TIME_END(n)
#define LOG_TIME(n)
#endif

#define LCD_WIDTH		(480)
#define LCD_HEIGHT		(800)

#define JOIN(x, y) JOIN_AGAIN(x, y)
#define JOIN_AGAIN(x, y) x ## y

//#define FRONT_CAM VGA
#define FRONT_CAM S5K6AAFX
#define BACK_CAM VGA

#if !defined (FRONT_CAM) || !defined(BACK_CAM)
#error "Please define the Camera module"
#endif

#define CE147_PREVIEW_WIDTH   (1280)
#define CE147_PREVIEW_HEIGHT   (720)
#define CE147_SNAPSHOT_WIDTH  (1600)
#define CE147_SNAPSHOT_HEIGHT (1200)

#define VGA_PREVIEW_WIDTH      (640)
#define VGA_PREVIEW_HEIGHT     (480)
#define VGA_SNAPSHOT_WIDTH     (640)
#define VGA_SNAPSHOT_HEIGHT    (480)

#define S5K4BA_PREVIEW_WIDTH      (800)
#define S5K4BA_PREVIEW_HEIGHT     (600)
#define S5K4BA_SNAPSHOT_WIDTH     (800)
#define S5K4BA_SNAPSHOT_HEIGHT    (600)

#define S5K6AAFX_PREVIEW_WIDTH      (640)
#define S5K6AAFX_PREVIEW_HEIGHT     (480)
#define S5K6AAFX_SNAPSHOT_WIDTH     (640)
#define S5K6AAFX_SNAPSHOT_HEIGHT    (480)

#define M5MO_PREVIEW_WIDTH      (800)
#define M5MO_PREVIEW_HEIGHT     (480)
#define M5MO_SNAPSHOT_WIDTH     (3264)
#define M5MO_SNAPSHOT_HEIGHT    (2448)


#define MAX_BACK_CAMERA_PREVIEW_WIDTH		JOIN(BACK_CAM,_PREVIEW_WIDTH)
#define MAX_BACK_CAMERA_PREVIEW_HEIGHT		JOIN(BACK_CAM,_PREVIEW_HEIGHT)
#define MAX_BACK_CAMERA_SNAPSHOT_WIDTH		JOIN(BACK_CAM,_SNAPSHOT_WIDTH)
#define MAX_BACK_CAMERA_SNAPSHOT_HEIGHT		JOIN(BACK_CAM,_SNAPSHOT_HEIGHT)

#define MAX_FRONT_CAMERA_PREVIEW_WIDTH		JOIN(FRONT_CAM,_PREVIEW_WIDTH)
#define MAX_FRONT_CAMERA_PREVIEW_HEIGHT		JOIN(FRONT_CAM,_PREVIEW_HEIGHT)
#define MAX_FRONT_CAMERA_SNAPSHOT_WIDTH		JOIN(FRONT_CAM,_SNAPSHOT_WIDTH)
#define MAX_FRONT_CAMERA_SNAPSHOT_HEIGHT	JOIN(FRONT_CAM,_SNAPSHOT_HEIGHT)


#define DEFAULT_JPEG_THUMBNAIL_WIDTH		(256)
#define DEFAULT_JPEG_THUMBNAIL_HEIGHT		(192)

#define CAMERA_DEV_NAME	  "/dev/video0"

#ifdef DUAL_PORT_RECORDING
#define CAMERA_DEV_NAME2  "/dev/video2"
#endif

#define BPP             (2)
#define MIN(x, y)       ((x < y) ? x : y)
#define MAX_BUFFERS     (8)

/*
 * V 4 L 2   F I M C   E X T E N S I O N S
 *
*/
#define V4L2_CID_ROTATION                   (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PADDR_Y                    (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_PADDR_CB                   (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_PADDR_CR                   (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_PADDR_CBCR                 (V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_STREAM_PAUSE               (V4L2_CID_PRIVATE_BASE + 53)

#define V4L2_CID_GET_UMP_SECURE_ID          (V4L2_CID_PRIVATE_BASE + 11)

#define V4L2_CID_CAM_JPEG_MAIN_SIZE         (V4L2_CID_PRIVATE_BASE + 32)
#define V4L2_CID_CAM_JPEG_MAIN_OFFSET       (V4L2_CID_PRIVATE_BASE + 33)
#define V4L2_CID_CAM_JPEG_THUMB_SIZE        (V4L2_CID_PRIVATE_BASE + 34)
#define V4L2_CID_CAM_JPEG_THUMB_OFFSET      (V4L2_CID_PRIVATE_BASE + 35)
#define V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET   (V4L2_CID_PRIVATE_BASE + 36)
#define V4L2_CID_CAM_JPEG_QUALITY           (V4L2_CID_PRIVATE_BASE + 37)

#define V4L2_PIX_FMT_YVYU       v4l2_fourcc('Y', 'V', 'Y', 'U')

/* FOURCC for FIMC specific */
#define V4L2_PIX_FMT_VYUY       v4l2_fourcc('V', 'Y', 'U', 'Y')
#define V4L2_PIX_FMT_NV16       v4l2_fourcc('N', 'V', '1', '6')
#define V4L2_PIX_FMT_NV61       v4l2_fourcc('N', 'V', '6', '1')
#define V4L2_PIX_FMT_NV12T      v4l2_fourcc('T', 'V', '1', '2')

/*
 * U S E R	 D E F I N E D   T Y P E S
 *
*/

struct fimc_buffer {
    void   *start;
    size_t  length;
};

struct yuv_fmt_list {
    const char   *name;
    const char   *desc;
    unsigned int  fmt;
    int           depth;
    int           planes;
};

class SecCamera {
public:
    enum CAMERA_ID {
        CAMERA_ID_BACK  = 0,
        CAMERA_ID_FRONT = 1,
    };

    enum AF_MODE {
        AF_MODE_BASE,
        AF_MODE_AUTO,
        AF_MODE_FIXED,
        AF_MODE_INFINITY,
        AF_MODE_MACRO,
        AF_MODE_MAX,
    };

    enum FRAME_RATE {
        FRAME_RATE_BASE = 5,
        FRAME_RATE_MAX  = 30,
    };

    enum SCENE_MODE {
        SCENE_MODE_BASE,
        SCENE_MODE_AUTO,
        SCENE_MODE_BEACH,
        SCENE_MODE_CANDLELIGHT,
        SCENE_MODE_FIREWORKS,
        SCENE_MODE_LANDSCAPE,
        SCENE_MODE_NIGHT,
        SCENE_MODE_NIGHTPORTRAIT,
        SCENE_MODE_PARTY,
        SCENE_MODE_PORTRAIT,
        SCENE_MODE_SNOW,
        SCENE_MODE_SPORTS,
        SCENE_MODE_STEADYPHOTO,
        SCENE_MODE_SUNSET,
        SCENE_MODE_MAX,
    };

    enum WHILTE_BALANCE {
        WHITE_BALANCE_BASE,
        WHITE_BALANCE_AUTO,
        WHITE_BALANCE_CLOUDY,
        WHITE_BALANCE_SUNNY,
        WHITE_BALANCE_FLUORESCENT,
        WHITE_BALANCE_INCANDESCENT,
        WHITE_BALANCE_OFF,
        WHITE_BALANCE_MAX,
    };

    enum IMAGE_EFFECT {
        IMAGE_EFFECT_BASE,
        IMAGE_EFFECT_ORIGINAL,
        IMAGE_EFFECT_AQUA,
        IMAGE_EFFECT_MONO,
        IMAGE_EFFECT_NEGATIVE,
        IMAGE_EFFECT_SEPIA,
        IMAGE_EFFECT_WHITEBOARD,
        IMAGE_EFFECT_MAX,
    };

    enum BRIGHTNESS {
        BRIGHTNESS_BASE   = 0,
        BRIGHTNESS_NORMAL = 4,
        BRIGHTNESS_MAX    = 9,
    };

    enum CONTRAST {
        CONTRAST_BASE   = 0,
        CONTRAST_NORMAL = 2,
        CONTRAST_MAX    = 4,
    };

    enum SHARPNESS {
        SHARPNESS_BASE  = 0,
        SHARPNESS_NOMAL = 2,
        SHARPNESS_MAX   = 4,
    };

    enum SATURATION {
        SATURATION_BASE  = 0,
        SATURATION_NOMAL = 2,
        SATURATION_MAX   = 4,
    };

    enum ZOOM {
        ZOOM_BASE =  0,
        ZOOM_MAX  = 10,
    };

    enum FLAG {
        FLAG_OFF = 0,
        FLAG_ON  = 1,
    };

private:
    int m_flag_create;

    int m_cam_fd;
#ifdef DUAL_PORT_RECORDING
    int m_cam_fd2;
    struct pollfd m_events_c2;
    int m_flag_record_start;
    struct fimc_buffer m_buffers_c2[MAX_BUFFERS];
#endif

#ifdef BOARD_SUPPORT_SYSMMU
    unsigned int secure_id[MAX_BUFFERS];
    ump_handle handle[MAX_BUFFERS];
    unsigned long vaddr_base[MAX_BUFFERS];
    unsigned int offset[MAX_BUFFERS];
#endif

    int m_camera_id;

    int m_preview_v4lformat;
    int m_preview_width;
    int m_preview_height;
    int m_preview_max_width;
    int m_preview_max_height;
    int m_recording_width;
    int m_recording_height;

    int m_snapshot_v4lformat;
    int m_snapshot_width;
    int m_snapshot_height;
    int m_snapshot_max_width;
    int m_snapshot_max_height;

    int m_frame_rate;

    int m_scene_mode;
    int m_white_balance;
    int m_image_effect;
    int m_brightness;
    int m_contrast;
    int m_sharpness;
    int m_saturation;
    int m_zoom;

    int m_angle;

    int m_af_mode;

    int m_flag_preview_start;
    int m_flag_current_info_changed;

    int m_current_camera_id;

    int m_current_frame_rate;
    int m_current_scene_mode;
    int m_current_white_balance;
    int m_current_image_effect;
    int m_current_brightness;
    int m_current_contrast;
    int m_current_sharpness;
    int m_current_saturation;
    int m_current_zoom;
    int m_current_af_mode;

    int m_jpeg_fd;
    int m_jpeg_thumbnail_width;
    int m_jpeg_thumbnail_height;
    int m_jpeg_quality;
    double       m_gps_latitude;
    double       m_gps_longitude;
    unsigned int m_gps_timestamp;
    short        m_gps_altitude;

    int m_nframe;
    struct fimc_buffer m_buffers_c[MAX_BUFFERS];
    struct pollfd m_events_c;

 public:

    SecCamera();
    ~SecCamera();

     static SecCamera * createInstance(void)
     {
	     static SecCamera singleton;
	     return &singleton;
     }

    status_t dump(int fd, const Vector<String16>& args);

    int               Create(int cameraId);
    void              Destroy();
    int               flagCreate(void) const;

    int               setCameraId(int camera_id);
    int               getCameraId(void);

    int               startPreview(void);
    int               stopPreview (void);
#ifdef DUAL_PORT_RECORDING
    int               startRecord(void);
    int               stopRecord (void);
    int               getRecord(void);
    unsigned int      getRecPhyAddrY(int);
    unsigned int      getRecPhyAddrC(int);
#endif
    void              releaseframe(int i);
    int             setRecordingSize(int width, int height);

    int               flagPreviewStart(void);
    int               getPreview(void);
    int               setPreviewSize(int   width, int	height, int pixel_format);
    int               getPreviewSize(int * width, int * height);
    int               getPreviewSize(int * width, int * height, unsigned int * frame_size);
    int               getPreviewMaxSize(int * width, int * height);
    int               getPreviewPixelFormat(void);
    int               setPreviewImage(int index, unsigned char * buffer, int size);

    int               setSnapshotSize(int	width, int	 height);
    int               getSnapshotSize(int * width, int * height);
    int               getSnapshotSize(int * width, int * height, int * frame_size);
    int               getSnapshotMaxSize(int * width, int * height);
    int               setSnapshotPixelFormat(int pixel_format);
    int               getSnapshotPixelFormat(void);

    unsigned char *   getJpeg  (unsigned char *snapshot_data, int snapshot_size, int * size);
    void              CloseJpegBuffer(void);
    unsigned char *   yuv2Jpeg (unsigned char * raw_data, int raw_size,
                                int * jpeg_size,
                                int width, int height, int pixel_format);

    int               setJpegThumbnailSize(int   width, int	height);
    int               getJpegThumbnailSize(int * width, int * height);

    int               setGpsInfo(double latitude, double longitude, unsigned int timestamp, int altitude);

    int               SetRotate(int angle);
    int               getRotate(void);

    void              setFrameRate   (int frame_rate);
    int               getFrameRate   (void);
    int               getFrameRateMin(void);
    int               getFrameRateMax(void);

    int               setSceneMode(int scene_mode);
    int               getSceneMode(void);

    int               setWhiteBalance(int white_balance);
    int               getWhiteBalance(void);

    int               setImageEffect(int image_effect);
    int               getImageEffect(void);

    int               setBrightness(int brightness);
    int               getBrightness(void);
    int               getBrightnessMin(void);
    int               getBrightnessMax(void);

    int               setContrast(int contrast);
    int               getContrast(void);
    int               getContrastMin(void);
    int               getContrastMax(void);

    int               setSaturation(int saturation);
    int               getSaturation(void);
    int               getSaturationMin(void);
    int               getSaturationMax(void);

    int               setSharpness(int sharpness);
    int               getSharpness(void);
    int               getSharpnessMin(void);
    int               getSharpnessMax(void);

    int               setZoom(int zoom);
    int               getZoom(void);
    int               getZoomMin(void);
    int               getZoomMax(void);

    int               setAFMode(int af_mode);
    int               getAFMode(void);
    int               runAF(int flag_on, int * flag_focused);

    void              setJpegQuality(int quality);
    unsigned char *   getJpeg(int*, unsigned int*);
    unsigned int      getSnapshot(unsigned char* rawBuffer, int pictureSize);
    int               getCameraFd(void);
    int               getJpegFd(void);
    void              setJpgAddr(unsigned char *addr);
    unsigned int      getPhyAddrY(int);
    unsigned int      getPhyAddrC(int);
#ifdef BOARD_SUPPORT_SYSMMU
    unsigned int      getSecureID(int);
    unsigned int      getOffset(int);
#endif
    void pausePreview();

private:
    int  m_resetCamera    (void);
    int  m_setCameraId    (int camera_id);
    int  m_setWidthHeightColorFormat(int fd, int colorformat, int width, int height);
    int  m_setFrameRate   (int frame_rate);
    int  m_setSceneMode   (int scene_mode);
    int  m_setWhiteBalance(int white_balance);
    int  m_setImageEffect (int image_effect);
    int  m_setBrightness  (int brightness);
    int  m_setContrast    (int contrast);
    int  m_setSharpness   (int sharpness);
    int  m_setSaturation  (int saturation);
    int  m_setZoom        (int zoom);
    int  m_setAF          (int af_mode, int flag_run, int flag_on, int * flag_focused);

    int  m_getCropRect    (unsigned int   src_width,  unsigned int   src_height,
                           unsigned int   dst_width,  unsigned int   dst_height,
                           unsigned int * crop_x,     unsigned int * crop_y,
                           unsigned int * crop_width, unsigned int * crop_height,
                           int            zoom);
    inline unsigned int m_frameSize(int format, int width, int height);
};

extern unsigned long measure_time(struct timeval *start, struct timeval *stop);

}; // namespace android

#endif // ANDROID_HARDWARE_CAMERA_SEC_H
