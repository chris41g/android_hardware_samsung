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

/*
************************************
* Filename: SecCamera.cpp
* Author:   Sachin P. Kamat
* Purpose:  This file interacts with the Camera and JPEG drivers.
*************************************
*/
//#define DUMP_SECJPEG
//#define LOG_NDEBUG 0
#define LOG_TAG "SecCamera"
#include <utils/Log.h>
#include "SecCamera.h"

#define CHECK(return_value)                                        \
                if((return_value) < 0)                               \
                {                                                  \
                        LOGE("%s::%d fail\n", __func__,__LINE__);  \
                        return -1;                                 \
                }                                                  \

#define CHECK_PTR(return_value)                                    \
                if((return_value) < 0)                               \
                {                                                  \
                        LOGE("%s::%d fail\n", __func__,__LINE__);  \
                        return NULL;                               \
                }                                                  \

#define ALIGN_TO_32B(x)   ((((x) + (1 <<  5) - 1) >>  5) <<  5)
#define ALIGN_TO_128B(x)  ((((x) + (1 <<  7) - 1) >>  7) <<  7)
#define ALIGN_TO_8KB(x)   ((((x) + (1 << 13) - 1) >> 13) << 13)
#define ALIGN_TO_64KB(x)   ((((x) + (1 << 16) - 1) >> 16) << 16)

namespace android {

// ======================================================================
// Camera controls

static struct timeval time_start;
static struct timeval time_stop;

unsigned long measure_time(struct timeval *start, struct timeval *stop)
{
    unsigned long sec, usec, time;

    sec = stop->tv_sec - start->tv_sec;

    if (stop->tv_usec >= start->tv_usec) {
        usec = stop->tv_usec - start->tv_usec;
    } else {
        usec = stop->tv_usec + 1000000 - start->tv_usec;
        sec--;
    }

    time = (sec * 1000000) + usec;

    return time;
}

static inline unsigned long check_performance()
{
    unsigned long time = 0;
    static unsigned long max=0, min=0xffffffff;

    if(time_start.tv_sec == 0 && time_start.tv_usec == 0) {
        gettimeofday(&time_start, NULL);
    } else {
        gettimeofday(&time_stop, NULL);
        time = measure_time(&time_start, &time_stop);
        if(max < time) max = time;
        if(min > time) min = time;
        LOGV("Interval: %lu us (%2.2lf fps), min:%2.2lf fps, max:%2.2lf fps\n", time, 1000000.0/time, 1000000.0/max, 1000000.0/min);
        gettimeofday(&time_start, NULL);
    }

    return time;
}

static int close_buffers(struct fimc_buffer *buffers)
{
    int i;

    for (i = 0; i < MAX_BUFFERS; i++) {
        if (buffers[i].start) {
            munmap(buffers[i].start, buffers[i].length);
            //LOGV("munmap():virt. addr[%d]: 0x%x size = %d\n", i, (unsigned int) buffers[i].start, buffers[i].length);
            buffers[i].start = NULL;
        }
    }

    return 0;
}

static int get_pixel_depth(unsigned int fmt)
{
    int depth = 0;

    switch (fmt) {
    case V4L2_PIX_FMT_NV12:
        depth = 12;
        break;
    case V4L2_PIX_FMT_NV12T:
        depth = 12;
        break;
    case V4L2_PIX_FMT_NV21:
        depth = 12;
        break;
    case V4L2_PIX_FMT_YUV420:
        depth = 12;
        break;

    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_VYUY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_YUV422P:
        depth = 16;
        break;

    case V4L2_PIX_FMT_RGB32:
        depth = 32;
        break;
    }

    return depth;
}

#define ALIGN_W(x)      ((x+0x7F)&(~0x7F)) // Set as multiple of 128
#define ALIGN_H(x)      ((x+0x1F)&(~0x1F)) // Set as multiple of 32
#define ALIGN_BUF(x)    ((x+0x1FFF)&(~0x1FFF)) // Set as multiple of 8K

static int init_yuv_buffers(struct fimc_buffer *buffers, int width, int height, unsigned int fmt)
{
    int i, len;

    len = (width * height * get_pixel_depth(fmt)) / 8;

    for (i = 0; i < MAX_BUFFERS; i++) {
        if(fmt==V4L2_PIX_FMT_NV12T) {
            buffers[i].start = NULL;
            buffers[i].length = ALIGN_BUF(ALIGN_W(width) * ALIGN_H(height))+ ALIGN_BUF(ALIGN_W(width) * ALIGN_H(height/2));
        } else {
            buffers[i].start = NULL;
            buffers[i].length = len;
        }
    }

    return 0;
}

static int fimc_poll(struct pollfd *events)
{
    int ret;

    ret = poll(events, 1, 5000);
    if (ret < 0) {
        LOGE("ERR(%s):poll error\n", __FUNCTION__);
        return ret;
    }

    if (ret == 0) {
        LOGE("ERR(%s):No data in 5 secs..\n", __FUNCTION__);
        return ret;
    }

    return ret;
}
#ifdef DUMP_YUV
static int save_yuv(struct fimc_buffer *m_buffers_c, int width, int height, int depth, int index, int frame_count)
{
    FILE *yuv_fp = NULL;
    char filename[100], *buffer = NULL;

    /* file create/open, note to "wb" */
    yuv_fp = fopen("/sdcard/camera_dump.yuv", "wb");
    if (yuv_fp==NULL) {
        LOGE("Save YUV] file open error");
        return -1;
    }

    buffer = (char *) malloc(m_buffers_c[index].length);
    if(buffer == NULL) {
        LOGE("Save YUV] buffer alloc failed");
        if(yuv_fp) fclose(yuv_fp);
        return -1;
    }

    memcpy(buffer, m_buffers_c[index].start, m_buffers_c[index].length);

    fflush(stdout);

    fwrite(buffer, 1, m_buffers_c[index].length, yuv_fp);

    fflush(yuv_fp);

    if(yuv_fp)
        fclose(yuv_fp);
    if(buffer)
        free(buffer);

    return 0;
}
#endif //DUMP_YUV

static int fimc_v4l2_querycap(int fp)
{
    struct v4l2_capability cap;
    int ret = 0;

    ret = ioctl(fp, VIDIOC_QUERYCAP, &cap);

    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_QUERYCAP failed\n", __FUNCTION__);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOGE("ERR(%s):no capture devices\n", __FUNCTION__);
        return -1;
    }

    return ret;
}

static int fimc_v4l2_enuminput(int fp, int camera_index)
{
    struct v4l2_input input;

    input.index = camera_index;

    if(ioctl(fp, VIDIOC_ENUMINPUT, &input) != 0) {
        LOGE("ERR(%s):No matching index found\n", __FUNCTION__);
        return -1;
    }
    LOGI("Name of input channel[%d] is %s\n", input.index, input.name);

    return input.index;
}


static int fimc_v4l2_s_input(int fp, int index)
{
    struct v4l2_input input;
    int ret;

    input.index = index;

    ret = ioctl(fp, VIDIOC_S_INPUT, &input);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_S_INPUT failed\n", __FUNCTION__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_s_fmt(int fp, int width, int height, unsigned int fmt, int flag_capture)
{
    struct v4l2_format v4l2_fmt;
    struct v4l2_pix_format pixfmt;
    int ret;

    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    memset(&pixfmt, 0, sizeof(pixfmt));

    pixfmt.width = width;
    pixfmt.height = height;
    pixfmt.pixelformat = fmt;

    pixfmt.sizeimage = (width * height * get_pixel_depth(fmt)) / 8;

    pixfmt.field = V4L2_FIELD_NONE;

    v4l2_fmt.fmt.pix = pixfmt;

    /* Set up for capture */
    ret = ioctl(fp, VIDIOC_S_FMT, &v4l2_fmt);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_S_FMT failed\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

static int fimc_v4l2_s_fmt_cap(int fp, int width, int height, unsigned int fmt)
{
    struct v4l2_format v4l2_fmt;
    struct v4l2_pix_format pixfmt;
    int ret;
    LOGV("++%s()", __FUNCTION__);

    memset(&pixfmt, 0, sizeof(pixfmt));

    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    pixfmt.width = width;
    pixfmt.height = height;
    pixfmt.pixelformat = fmt;
    pixfmt.colorspace = V4L2_COLORSPACE_JPEG;

    pixfmt.sizeimage = (width * height * get_pixel_depth(fmt)) / 8;

    v4l2_fmt.fmt.pix = pixfmt;

    /* Set up for capture */
    ret = ioctl(fp, VIDIOC_S_FMT, &v4l2_fmt);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_S_FMT failed\n", __FUNCTION__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_enum_fmt(int fp, unsigned int fmt)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;

    while (ioctl(fp, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        if (fmtdesc.pixelformat == fmt) {
            LOGI("passed fmt = %d found pixel format[%d]: %s\n", fmt, fmtdesc.index, fmtdesc.description);
            found = 1;
            break;
        }

        fmtdesc.index++;
    }

    if (!found) {
        LOGE("unsupported pixel format\n");
        return -1;
    }

    return 0;
}

static int fimc_v4l2_reqbufs(int fp, enum v4l2_buf_type type, int nr_bufs)
{
        struct v4l2_requestbuffers req;
        int ret;

        req.count = nr_bufs;
        req.type = type;
        req.memory = V4L2_MEMORY_MMAP;

        ret = ioctl(fp, VIDIOC_REQBUFS, &req);
        if(ret < 0) {
        LOGE("ERR(%s):VIDIOC_REQBUFS failed\n", __FUNCTION__);
                return -1;
        }

        return req.count;
}

static int fimc_v4l2_querybuf(int fp, struct fimc_buffer *buffers, enum v4l2_buf_type type, int nr_frames)
{
    struct v4l2_buffer v4l2_buf;
    int i, ret;

    for(i = 0; i < nr_frames; i++) {
        v4l2_buf.type = type;
        v4l2_buf.memory = V4L2_MEMORY_MMAP;
        v4l2_buf.index = i;

        ret = ioctl(fp, VIDIOC_QUERYBUF, &v4l2_buf);
        if(ret < 0) {
            LOGE("ERR(%s):VIDIOC_QUERYBUF failed\n", __FUNCTION__);
            return -1;
        }

        buffers[i].length = v4l2_buf.length;

        if (nr_frames == 1) {
            buffers[i].length = v4l2_buf.length;
            if ((buffers[i].start = (char *)mmap(0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fp, v4l2_buf.m.offset)) < 0) {
                LOGE("%s %d] mmap() failed\n",__func__, __LINE__);
                return -1;
            }

            LOGV("buffers[%d].start = %p v4l2_buf.length = %d", i, buffers[i].start, v4l2_buf.length);
        } else {

#if defined(DUMP_YUV)
            buffers[i].length = v4l2_buf.length;
            if ((buffers[i].start = (char *)mmap(0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                    fp, v4l2_buf.m.offset)) < 0) {
                LOGE("%s %d] mmap() failed\n",__func__, __LINE__);
                return -1;
            }

            //LOGV("buffers[%d].start = %p v4l2_buf.length = %d", i, buffers[i].start, v4l2_buf.length);
#endif
        }

    }

    return 0;
}

static int fimc_v4l2_streamon(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(fp, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_STREAMON failed\n", __FUNCTION__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_streamoff(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(fp, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_STREAMOFF failed\n", __FUNCTION__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_qbuf(int fp, int index)
{
    struct v4l2_buffer v4l2_buf;
    int ret;

    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    v4l2_buf.index = index;

    ret = ioctl(fp, VIDIOC_QBUF, &v4l2_buf);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_QBUF failed\n", __FUNCTION__);
        return ret;
    }

    return 0;
}

static int fimc_v4l2_dqbuf(int fp)
{
    struct v4l2_buffer v4l2_buf;
    int ret;

    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(fp, VIDIOC_DQBUF, &v4l2_buf);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_DQBUF failed\n", __FUNCTION__);
        return ret;
    }

    return v4l2_buf.index;
}

static int fimc_v4l2_blk_dqbuf(int fp, struct pollfd *events)
{
    struct v4l2_buffer v4l2_buf;
    int index;
    int ret = 0;

    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(fp, VIDIOC_DQBUF, &v4l2_buf);
    if (ret < 0) {
        LOGV("VIDIOC_DQBUF first is empty") ;
        fimc_poll(events);
        return fimc_v4l2_dqbuf(fp);
    } else {
        index = v4l2_buf.index;
        while(ret == 0)
        {
            ret = ioctl(fp, VIDIOC_DQBUF, &v4l2_buf);
            if (ret == 0) {
                LOGV("VIDIOC_DQBUF is not still empty %d",v4l2_buf.index);
                fimc_v4l2_qbuf(fp, index);
                index = v4l2_buf.index;
            } else {
                LOGV("VIDIOC_DQBUF is empty now %d ",index);
                return index;
            }
        }
    }
    return v4l2_buf.index;
}

static int fimc_v4l2_g_ctrl(int fp, unsigned int id)
{
    struct v4l2_control ctrl;
    int ret;

    ctrl.id = id;

    ret = ioctl(fp, VIDIOC_G_CTRL, &ctrl);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_G_CTRL failed\n", __FUNCTION__);
        return ret;
    }

    return ctrl.value;
}

static int fimc_v4l2_s_ctrl(int fp, unsigned int id, unsigned int value)
{
    struct v4l2_control ctrl;
    int ret;

    ctrl.id = id;
    ctrl.value = value;

    ret = ioctl(fp, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_S_CTRL failed\n", __FUNCTION__);
        return ret;
    }

    return ctrl.value;
}

static int fimc_v4l2_g_parm(int fp)
{
    struct v4l2_streamparm stream;
    int ret;

    stream.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fp, VIDIOC_G_PARM, &stream);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_G_PARM failed\n", __FUNCTION__);
        return -1;
    }

    /*
       LOGV("timeperframe: numerator %d, denominator %d\n",
       stream.parm.capture.timeperframe.numerator,
       stream.parm.capture.timeperframe.denominator);
     */

    return 0;
}

static int fimc_v4l2_s_parm(int fp, int fps_numerator, int fps_denominator)
{
    struct v4l2_streamparm stream;
    int ret;

    stream.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream.parm.capture.capturemode = 0;
    stream.parm.capture.timeperframe.numerator = fps_numerator;
    stream.parm.capture.timeperframe.denominator = fps_denominator;

    ret = ioctl(fp, VIDIOC_S_PARM, &stream);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_S_PARM failed\n", __FUNCTION__);
        return ret;
    }

    return 0;
}

#if 0
static int fimc_v4l2_s_parm_ex(int fp, int mode, int no_dma_op) //Kamat: not present in new code
{
    struct v4l2_streamparm stream;
    int ret;

    stream.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream.parm.capture.capturemode = mode;
    if(no_dma_op)
        stream.parm.capture.reserved[0] = 100;

    ret = ioctl(fp, VIDIOC_S_PARM, &stream);
    if (ret < 0) {
        LOGE("ERR(%s):VIDIOC_S_PARM_EX failed\n", __FUNCTION__);
        return ret;
    }

    return 0;
}
#endif

// ======================================================================
// Constructor & Destructor

SecCamera::SecCamera() :
        m_flag_create          (FLAG_OFF),
        m_camera_id            (CAMERA_ID_BACK),
        m_preview_v4lformat    (-1),
        m_preview_width        (MAX_BACK_CAMERA_PREVIEW_WIDTH),
        m_preview_height       (MAX_BACK_CAMERA_PREVIEW_HEIGHT),
        m_preview_max_width    (MAX_BACK_CAMERA_PREVIEW_WIDTH),
        m_preview_max_height   (MAX_BACK_CAMERA_PREVIEW_HEIGHT),
        m_snapshot_v4lformat   (-1),
        m_snapshot_width       (MAX_BACK_CAMERA_SNAPSHOT_WIDTH),
        m_snapshot_height      (MAX_BACK_CAMERA_SNAPSHOT_HEIGHT),
        m_snapshot_max_width   (MAX_BACK_CAMERA_SNAPSHOT_WIDTH),
        m_snapshot_max_height  (MAX_BACK_CAMERA_SNAPSHOT_HEIGHT),
        m_frame_rate           (FRAME_RATE_MAX),
        m_scene_mode           (SCENE_MODE_AUTO),
        m_white_balance        (WHITE_BALANCE_AUTO),
        m_image_effect         (IMAGE_EFFECT_ORIGINAL),
        m_brightness           (BRIGHTNESS_NORMAL),
        m_contrast             (CONTRAST_NORMAL),
        m_sharpness            (SHARPNESS_NOMAL),
        m_saturation           (SATURATION_NOMAL),
        m_zoom                 (ZOOM_BASE),
        m_angle                (0),
        m_af_mode              (AF_MODE_AUTO),
        m_flag_preview_start    (FLAG_OFF),
        m_flag_current_info_changed(FLAG_ON),
        m_current_camera_id    (-1),
        m_current_frame_rate   (-1),
        m_current_scene_mode   (SCENE_MODE_AUTO),
        m_current_white_balance(WHITE_BALANCE_AUTO),
        m_current_image_effect (IMAGE_EFFECT_ORIGINAL),
        m_current_brightness   (BRIGHTNESS_NORMAL),
        m_current_contrast     (CONTRAST_NORMAL),
        m_current_sharpness    (SHARPNESS_NOMAL),
        m_current_saturation   (SATURATION_NOMAL),
        m_current_zoom         (ZOOM_BASE),
        m_current_af_mode      (AF_MODE_BASE),
        m_jpeg_thumbnail_width (0),
        m_jpeg_thumbnail_height(0),
        m_jpeg_quality         (99),
        m_gps_latitude         (0.0f),
        m_gps_longitude        (0.0f),
        m_gps_timestamp        (0),
        m_gps_altitude         (169),
        m_nframe                (1)
{
    LOGV("%s()", __FUNCTION__);

//    if(this->Create() < 0)
//        LOGE("ERR(%s):Create() fail\n", __FUNCTION__);
}

SecCamera::~SecCamera()
{
    LOGV("%s()", __FUNCTION__);

    this->Destroy();
}

void SecCamera::releaseframe(int i)
{
    LOGV("releaseframe : (%d)",i);

#ifdef DUAL_PORT_RECORDING
    int ret = fimc_v4l2_qbuf(m_cam_fd2, i);
#endif
 //  CHECK(ret);

}

int SecCamera::Create(int cameraId)
{
    LOGV("%s()", __FUNCTION__);
    int ret = 0, index = 0;

    if(m_flag_create == FLAG_OFF) {

        m_cam_fd = open(CAMERA_DEV_NAME, O_RDWR);
        if (m_cam_fd < 0) {
            LOGE("ERR(%s):Cannot open %s (error : %s)\n", __FUNCTION__, CAMERA_DEV_NAME, strerror(errno));

            return -1;
        }
        LOGV("(%s):m_cam_fd device open ID = %d\n", __FUNCTION__,m_cam_fd);

        int camera_index = 0;

        if(cameraId == CAMERA_ID_FRONT)
            camera_index = 1;
        else // if(cameraId == CAMERA_ID_BACK)
            camera_index = 0;

        LOGV("camera_index %d",camera_index);
        ret = fimc_v4l2_querycap(m_cam_fd);
        CHECK(ret);
        index = fimc_v4l2_enuminput(m_cam_fd, camera_index);
        CHECK(index);
        ret = fimc_v4l2_s_input(m_cam_fd, index);
        CHECK(ret);

#ifdef DUAL_PORT_RECORDING
        m_cam_fd2 = open(CAMERA_DEV_NAME2, O_RDWR);
        if (m_cam_fd2 < 0) {
            LOGE("ERR(%s):Cannot open %s (error : %s)\n", __FUNCTION__, CAMERA_DEV_NAME2, strerror(errno));
            return -1;
        }

        LOGV("m_cam_fd2(%d)", m_cam_fd2);

        ret = fimc_v4l2_querycap(m_cam_fd2);
        CHECK(ret);
        index = fimc_v4l2_enuminput(m_cam_fd2, camera_index);
        CHECK(index);
        ret = fimc_v4l2_s_input(m_cam_fd2, index);
        CHECK(ret);
#endif
        m_camera_id = index;
        m_flag_create = FLAG_ON;
    }
    return 0;
}

void SecCamera::Destroy()
{
    LOGV("%s()", __FUNCTION__);

    if(m_flag_create == FLAG_ON) {
        LOGE_IF(this->stopPreview() < 0,
                "ERR(%s):Fail on stopPreview()\n", __FUNCTION__);

#ifdef DUAL_PORT_RECORDING
        stopRecord();
#endif


        if(m_jpeg_fd > 0) {
            LOGE_IF(api_jpeg_encode_deinit(m_jpeg_fd) != JPEG_OK,
                    "ERR(%s):Fail on api_jpeg_encode_deinit\n", __FUNCTION__);
            m_jpeg_fd = 0;
        }

        LOGV("m_cam_fd(%d)", m_cam_fd);
        if(m_cam_fd > 0) {
            close(m_cam_fd);
            m_cam_fd = 0;
        }
#ifdef DUAL_PORT_RECORDING
        LOGV("m_cam_fd2(%d)", m_cam_fd2);
        if(m_cam_fd2 > 0) {
            close(m_cam_fd2);
            m_cam_fd2 = 0;
        }
#endif
        m_flag_create = FLAG_OFF;
    }
}


int SecCamera::flagCreate(void) const
{
    LOGV("%s() : %d", __FUNCTION__, m_flag_create);

    if(m_flag_create == FLAG_ON)
        return 1;
    else //if(m_flag_create == FLAG_OFF)
        return 0;
}


int SecCamera::getCameraFd(void)
{
    return m_cam_fd;
}

// ======================================================================
// Preview

int SecCamera::startPreview(void)
{
    LOGI("++%s() \n", __FUNCTION__);

    // aleady started
    if(m_flag_preview_start == FLAG_ON) {
        LOGE("ERR(%s):Preview was already started\n", __FUNCTION__);
        return 0;
    }

    int ret = 0;

    memset(&m_events_c, 0, sizeof(m_events_c));
    m_events_c.fd = m_cam_fd;
    m_events_c.events = POLLIN | POLLERR;

    ret = fimc_v4l2_enum_fmt(m_cam_fd,m_preview_v4lformat);
    CHECK(ret);
    ret = fimc_v4l2_s_fmt(m_cam_fd, m_preview_width, m_preview_height, m_preview_v4lformat, 0);
    CHECK(ret);

    LOGI("%s:: m_preview_width = 0x%x, m_preview_height = 0x%x \n",
        __func__, m_preview_width, m_preview_height);

    if(m_resetCamera() < 0) {
        LOGE("ERR(%s):m_resetCamera() fail\n", __FUNCTION__);
        return -1;
    }

    init_yuv_buffers(m_buffers_c, m_preview_width, m_preview_height, m_preview_v4lformat);
    ret = fimc_v4l2_reqbufs(m_cam_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, MAX_BUFFERS);
    CHECK(ret);
    ret = fimc_v4l2_querybuf(m_cam_fd, m_buffers_c, V4L2_BUF_TYPE_VIDEO_CAPTURE, MAX_BUFFERS);
    CHECK(ret);

    /* start with all buffers in queue */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        ret = fimc_v4l2_qbuf(m_cam_fd, i);
        CHECK(ret);
    }

    ret = fimc_v4l2_streamon(m_cam_fd);
    CHECK(ret);

    ret = fimc_poll(&m_events_c);
    CHECK(ret);
    int index = fimc_v4l2_dqbuf(m_cam_fd);

    ret = fimc_v4l2_qbuf(m_cam_fd, index);
    CHECK(ret);

    m_flag_preview_start = FLAG_ON;
    LOGI("--%s() \n", __FUNCTION__);

    return 0;
}

int SecCamera::stopPreview(void)
{
    LOGV("++%s() \n", __FUNCTION__);

    if(m_flag_preview_start == FLAG_OFF)
        return 0;

    if(m_cam_fd <= 0) {
        LOGE("ERR(%s):Camera was closed\n", __FUNCTION__);
        return -1;
    }

    close_buffers(m_buffers_c);

    int ret = fimc_v4l2_streamoff(m_cam_fd);

    m_flag_preview_start = FLAG_OFF; //Kamat check
    m_flag_current_info_changed = FLAG_ON;
    CHECK(ret);

    LOGV("--%s() \n", __FUNCTION__);
    return 0;
}

int SecCamera::flagPreviewStart(void)
{
    LOGV("%s:started(%d)", __func__, m_flag_preview_start);

    return m_flag_preview_start;
}

//Recording
#ifdef DUAL_PORT_RECORDING
int SecCamera::startRecord(void)
{
    LOGI("++%s() \n", __FUNCTION__);
    int ret = 0;
    // aleady started
    if(m_flag_record_start == FLAG_ON) {
        LOGE("ERR(%s):Preview was already started\n", __FUNCTION__);
        return 0;
    }

    if (m_cam_fd2 <= 0) {
        LOGE("ERR(%s):Camera was closed\n", __func__);
        return -1;
    }

    memset(&m_events_c2, 0, sizeof(m_events_c2));
    m_events_c2.fd = m_cam_fd2;
    m_events_c2.events = POLLIN | POLLERR;

    int color_format = V4L2_PIX_FMT_NV12;

    ret = fimc_v4l2_enum_fmt(m_cam_fd2,color_format);
    CHECK(ret);
    LOGV("%s: m_recording_width = %d, m_recording_height = %d\n", __func__, m_preview_width, m_preview_height);

    ret = fimc_v4l2_s_fmt(m_cam_fd2, m_preview_width, m_preview_height, color_format, 0);
    CHECK(ret);
/*
    if(m_resetCamera() < 0) {
        LOGE("ERR(%s):m_resetCamera() fail\n", __FUNCTION__);
        return -1;
    }
*/

    init_yuv_buffers(m_buffers_c2, m_preview_width, m_preview_height, color_format);
    ret = fimc_v4l2_reqbufs(m_cam_fd2, V4L2_BUF_TYPE_VIDEO_CAPTURE, MAX_BUFFERS);
    CHECK(ret);

    ret = fimc_v4l2_querybuf(m_cam_fd2, m_buffers_c2, V4L2_BUF_TYPE_VIDEO_CAPTURE, MAX_BUFFERS);
    CHECK(ret);

    /* start with all buffers in queue */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        ret = fimc_v4l2_qbuf(m_cam_fd2, i);
        CHECK(ret);
    }

    ret = fimc_v4l2_streamon(m_cam_fd2);
    CHECK(ret);

    // It is a delay for a new frame, not to show the previous bigger ugly picture frame.
    ret = fimc_poll(&m_events_c2);
    CHECK(ret);
    int index = fimc_v4l2_dqbuf(m_cam_fd2);

    ret = fimc_v4l2_qbuf(m_cam_fd2, index);
    CHECK(ret);

    m_flag_record_start = FLAG_ON;

    LOGI("--%s() \n", __FUNCTION__);

    return 0;
}

int SecCamera::stopRecord(void)
{
    LOGI("++%s() \n", __FUNCTION__);


    if(m_flag_record_start == FLAG_OFF)
        return 0;

    close_buffers(m_buffers_c2);

    if(m_cam_fd2 <= 0) {
        LOGE("ERR(%s):Camera was closed\n", __FUNCTION__);
        return -1;
    }

    int ret = fimc_v4l2_streamoff(m_cam_fd2);

    m_flag_record_start = FLAG_OFF; //Kamat check
    CHECK(ret);

    LOGI("--%s() \n", __FUNCTION__);

    return 0;
}

unsigned int SecCamera::getRecPhyAddrY(int index)
{
    unsigned int addr_y;

    addr_y = fimc_v4l2_s_ctrl(m_cam_fd2, V4L2_CID_PADDR_Y, index);
    CHECK((int)addr_y);
    return addr_y;
}

unsigned int SecCamera::getRecPhyAddrC(int index)
{
    unsigned int addr_c;

    addr_c = fimc_v4l2_s_ctrl(m_cam_fd2, V4L2_CID_PADDR_CBCR, index);
    CHECK((int)addr_c);
    return addr_c;
}
#endif //DUAL_PORT_RECORDING

unsigned int SecCamera::getPhyAddrY(int index)
{
    unsigned int addr_y;

    addr_y = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_PADDR_Y, index);
    return addr_y;
}

unsigned int SecCamera::getPhyAddrC(int index)
{
    unsigned int addr_c;

    addr_c = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_PADDR_CBCR, index);
    return addr_c;
}

void SecCamera::pausePreview()
{
    fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_STREAM_PAUSE, 0);
}

int SecCamera::getPreview()
{
    int index;

    if(m_resetCamera() < 0) {
        LOGE("ERR(%s):m_resetCamera() fail\n", __FUNCTION__);
        return -1;
    }

#ifdef PERFORMANCE

    LOG_TIME_DEFINE(0)
    LOG_TIME_DEFINE(1)

    LOG_TIME_START(0)
    //    fimc_poll(&m_events_c);
    LOG_TIME_END(0)
    LOG_CAMERA("fimc_poll interval: %lu us", LOG_TIME(0));

    LOG_TIME_START(1)
    index = fimc_v4l2_dqbuf(m_cam_fd);
    LOG_TIME_END(1)
    LOG_CAMERA("fimc_dqbuf interval: %lu us", LOG_TIME(1));

#else
//    fimc_poll(&m_events_c);
//    index = fimc_v4l2_dqbuf(m_cam_fd);
    index = fimc_v4l2_blk_dqbuf(m_cam_fd, &m_events_c);
#endif
    if(!(0 <= index && index < MAX_BUFFERS)) {
        LOGE("ERR(%s):wrong index = %d\n", __FUNCTION__, index);
        return -1;
    }

    int ret = fimc_v4l2_qbuf(m_cam_fd, index); //Kamat: is it overhead?
    CHECK(ret);

    return index;

}

#ifdef DUAL_PORT_RECORDING
int SecCamera::getRecord()
{
    int index;

#ifdef PERFORMANCE
    LOG_TIME_DEFINE(0)
    LOG_TIME_DEFINE(1)

    LOG_TIME_START(0)
    fimc_poll(&m_events_c2);
    LOG_TIME_END(0)
    LOG_CAMERA("fimc_poll interval: %lu us", LOG_TIME(0));

    LOG_TIME_START(1)
    index = fimc_v4l2_dqbuf(m_cam_fd2);
    LOG_TIME_END(1)
    LOG_CAMERA("fimc_dqbuf interval: %lu us", LOG_TIME(1));
#else
    fimc_poll(&m_events_c2);
    index = fimc_v4l2_dqbuf(m_cam_fd2);
#endif
    if(!(0 <= index && index < MAX_BUFFERS)) {
        LOGE("ERR(%s):wrong index = %d\n", __FUNCTION__, index);
        return -1;
    }

    //int ret = fimc_v4l2_qbuf(m_cam_fd2, index); //Kamat: is it overhead?
    //CHECK(ret);

    return index;
}
#endif //DUAL_PORT_RECORDING

int SecCamera::setPreviewSize(int width, int height, int pixel_format)
{
    LOGI("%s(width(%d), height(%d), format(%d))", __FUNCTION__, width, height, pixel_format);

    int v4lpixelformat = pixel_format;

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
    if(v4lpixelformat == V4L2_PIX_FMT_YUV420)  { LOGV("PreviewFormat:V4L2_PIX_FMT_YUV420"); }
    else if(v4lpixelformat == V4L2_PIX_FMT_NV12)    { LOGV("PreviewFormat:V4L2_PIX_FMT_NV12"); }
    else if(v4lpixelformat == V4L2_PIX_FMT_NV12T)   { LOGV("PreviewFormat:V4L2_PIX_FMT_NV12T"); }
    else if(v4lpixelformat == V4L2_PIX_FMT_NV21)    { LOGV("PreviewFormat:V4L2_PIX_FMT_NV21"); }
    else if(v4lpixelformat == V4L2_PIX_FMT_YUV422P) { LOGV("PreviewFormat:V4L2_PIX_FMT_YUV422P"); }
    else if(v4lpixelformat == V4L2_PIX_FMT_YUYV)    { LOGV("PreviewFormat:V4L2_PIX_FMT_YUYV"); }
    else if(v4lpixelformat == V4L2_PIX_FMT_RGB565)  { LOGV("PreviewFormat:V4L2_PIX_FMT_RGB565"); }
    else { LOGV("PreviewFormat:UnknownFormat"); }
#endif

    m_preview_width  = width;
    m_preview_height = height;
    m_preview_v4lformat = v4lpixelformat;

    return 0;
}

int SecCamera::getPreviewSize(int * width, int * height)
{
    *width    = m_preview_width;
    *height = m_preview_height;

    return 0;
}

int SecCamera::getPreviewSize(int * width, int * height, unsigned int * frame_size)
{
    *width    = m_preview_width;
    *height = m_preview_height;
    *frame_size = m_frameSize(m_preview_v4lformat, m_preview_width, m_preview_height);

    return 0;
}

int SecCamera::getPreviewMaxSize(int * width, int * height)
{
    *width    = m_preview_max_width;
    *height = m_preview_max_height;

    return 0;
}

int SecCamera::getPreviewPixelFormat(void)
{
    return m_preview_v4lformat;
}


// ======================================================================
// Snapshot
void SecCamera::CloseJpegBuffer(void)
{
    close_buffers(m_buffers_c);
}

int SecCamera::getJpegFd(void)
{
    return m_jpeg_fd;
}

void SecCamera::setJpgAddr(unsigned char *addr)
{
    //SetMapAddr(addr);
}

unsigned int SecCamera::getSnapshot(unsigned char *rawBuffer, int pictureSize)
{
    LOGI("++%s() \n", __FUNCTION__);

    int index;
    unsigned int addr;
    struct fimc_buffer buffers_c[MAX_BUFFERS];
    LOG_TIME_DEFINE(0)
    LOG_TIME_DEFINE(1)
    LOG_TIME_DEFINE(2)
    LOG_TIME_DEFINE(3)
    LOG_TIME_DEFINE(4)
    LOG_TIME_DEFINE(5)

    LOG_TIME_START(0)
    stopPreview();
    LOG_TIME_END(0)

    memset(&m_events_c, 0, sizeof(m_events_c));
    m_events_c.fd = m_cam_fd;
    m_events_c.events = POLLIN | POLLERR;

    fimc_v4l2_enum_fmt(m_cam_fd,m_snapshot_v4lformat);

    LOGV("%s: width = %d, height = %d\n", __func__, m_snapshot_width, m_snapshot_height);
    fimc_v4l2_s_fmt(m_cam_fd, m_snapshot_width, m_snapshot_height, m_snapshot_v4lformat, 0);

    if(m_resetCamera() < 0) {
        LOGE("ERR(%s):m_resetCamera() fail\n", __FUNCTION__);
        return 0;
    }

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
    if(m_snapshot_v4lformat == V4L2_PIX_FMT_YUV420) { LOGV("SnapshotFormat:V4L2_PIX_FMT_YUV420"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_NV12) { LOGV("SnapshotFormat:V4L2_PIX_FMT_NV12"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_NV12T) { LOGV("SnapshotFormat:V4L2_PIX_FMT_NV12T"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_NV21) { LOGV("SnapshotFormat:V4L2_PIX_FMT_NV21"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_YUV422P) { LOGV("SnapshotFormat:V4L2_PIX_FMT_YUV422P"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_YUYV) { LOGV("SnapshotFormat:V4L2_PIX_FMT_YUYV"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_UYVY) { LOGV("SnapshotFormat:V4L2_PIX_FMT_UYVY"); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565) { LOGV("SnapshotFormat:V4L2_PIX_FMT_RGB565"); }
    else { LOGV("SnapshotFormat:UnknownFormat"); }
#endif

    LOG_TIME_START(1) // prepare
    m_nframe = 1;

    init_yuv_buffers(m_buffers_c, m_snapshot_width, m_snapshot_height, m_snapshot_v4lformat);
    fimc_v4l2_reqbufs(m_cam_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, m_nframe);
    fimc_v4l2_querybuf(m_cam_fd, m_buffers_c, V4L2_BUF_TYPE_VIDEO_CAPTURE, m_nframe);

    fimc_v4l2_qbuf(m_cam_fd, 0);

    fimc_v4l2_streamon(m_cam_fd);
    LOG_TIME_END(1)

    LOG_TIME_START(2) // capture
    fimc_poll(&m_events_c);

    for(int aa = 0 ; aa < 10 ; aa++)
    {
        fimc_poll(&m_events_c);
        index = fimc_v4l2_dqbuf(m_cam_fd);
        fimc_v4l2_qbuf(m_cam_fd, index);
    }

    fimc_poll(&m_events_c);
    index = fimc_v4l2_dqbuf(m_cam_fd);

    LOGI("snapshot dqueued buffer = %d snapshot_width = %d snapshot_height = %d",
            index, m_snapshot_width, m_snapshot_height);

#ifdef DUMP_YUV
    save_yuv(m_buffers_c, m_snapshot_width, m_snapshot_height, 16, index, 0);
#endif
    LOG_TIME_END(2)

    addr = getPhyAddrY(index);
    if(addr == 0)
        LOGE("%s] Physical address 0", __FUNCTION__);

    LOGI("phyaddr = 0x%x, index = %d, m_buffers_c[%d].start = 0x%p",
            addr, index, index, m_buffers_c[index].start);
    memcpy(rawBuffer, m_buffers_c[index].start, pictureSize);

    fimc_v4l2_streamoff(m_cam_fd);

    close_buffers(m_buffers_c);

    LOG_CAMERA("getSnapshot intervals : stopPreview(%lu), prepare(%lu), capture(%lu), "
            "memcpy(%lu), yuv2Jpeg(%lu), post(%lu)  us",
            LOG_TIME(0), LOG_TIME(1), LOG_TIME(2),
            LOG_TIME(3), LOG_TIME(4), LOG_TIME(5));

    LOGI("--%s() \n", __FUNCTION__);

    return addr;
}

int SecCamera::setSnapshotSize(int width, int height)
{
    LOGI("%s(width(%d), height(%d))", __FUNCTION__, width, height);

    m_snapshot_width  = width;
    m_snapshot_height = height;

    return 0;
}

int SecCamera::getSnapshotSize(int * width, int * height)
{
    *width    = m_snapshot_width;
    *height = m_snapshot_height;

    return 0;
}

int SecCamera::getSnapshotSize(int * width, int * height, int * frame_size)
{
    *width    = m_snapshot_width;
    *height = m_snapshot_height;
    *frame_size = m_frameSize(m_snapshot_v4lformat, m_snapshot_width, m_snapshot_height);

    return 0;
}

int SecCamera::getSnapshotMaxSize(int * width, int * height)
{
    switch(m_camera_id) {
        case CAMERA_ID_FRONT:
            m_snapshot_max_width  = MAX_FRONT_CAMERA_SNAPSHOT_WIDTH;
            m_snapshot_max_height = MAX_FRONT_CAMERA_SNAPSHOT_HEIGHT;
            break;

        case CAMERA_ID_BACK:
        default:

            m_snapshot_max_width  = MAX_BACK_CAMERA_SNAPSHOT_WIDTH;
            m_snapshot_max_height = MAX_BACK_CAMERA_SNAPSHOT_HEIGHT;
            break;
    }

    *width    = m_snapshot_max_width;
    *height = m_snapshot_max_height;

    return 0;
}

int SecCamera::setSnapshotPixelFormat(int pixel_format)
{
    int v4lpixelformat= pixel_format;

    if(m_snapshot_v4lformat != v4lpixelformat)
        m_snapshot_v4lformat = v4lpixelformat;

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
    if(m_snapshot_v4lformat == V4L2_PIX_FMT_YUV420) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_YUV420", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_NV12) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_NV12", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_NV12T) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_NV12T", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_NV21) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_NV21", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_YUV422P) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_YUV422P", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_YUYV) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_YUYV", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_UYVY) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_UYVY", __FUNCTION__); }
    else if(m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565) { LOGV("%s():SnapshotFormat:V4L2_PIX_FMT_RGB565", __FUNCTION__); }
    else { LOGV("SnapshotFormat:UnknownFormat"); }
#endif

    return 0;
}

int SecCamera::getSnapshotPixelFormat(void)
{
    return m_snapshot_v4lformat;
}


// ======================================================================
// Settings

int SecCamera::setCameraId(int camera_id)
{
    if(m_camera_id == camera_id)
        return 0;

    LOGV("%s(camera_id(%d))", __FUNCTION__, camera_id);

    switch(camera_id) {
        case CAMERA_ID_FRONT:
            m_preview_max_width   = MAX_FRONT_CAMERA_PREVIEW_WIDTH;
            m_preview_max_height  = MAX_FRONT_CAMERA_PREVIEW_HEIGHT;
            m_snapshot_max_width  = MAX_FRONT_CAMERA_SNAPSHOT_WIDTH;
            m_snapshot_max_height = MAX_FRONT_CAMERA_SNAPSHOT_HEIGHT;
            break;

        case CAMERA_ID_BACK:
            m_preview_max_width   = MAX_BACK_CAMERA_PREVIEW_WIDTH;
            m_preview_max_height  = MAX_BACK_CAMERA_PREVIEW_HEIGHT;
            m_snapshot_max_width  = MAX_BACK_CAMERA_SNAPSHOT_WIDTH;
            m_snapshot_max_height = MAX_BACK_CAMERA_SNAPSHOT_HEIGHT;
            break;
        default:
            LOGE("ERR(%s)::Invalid camera id(%d)\n", __func__, camera_id);
            return -1;
    }

    m_camera_id = camera_id;
    m_flag_current_info_changed = FLAG_ON;

    return 0;
}

int SecCamera::getCameraId(void)
{
    return m_camera_id;
}

// -----------------------------------

int SecCamera::SetRotate(int angle)
{
    LOGI("%s(angle(%d))", __FUNCTION__, angle);

    unsigned int id;

    switch(angle) {
        case -360 :
        case    0 :
        case  360 :
            //id = V4L2_CID_ROTATE_ORIGINAL;
            m_angle = 0;
            break;

        case -270 :
        case   90 :
            //id = V4L2_CID_ROTATE_90;
            m_angle = 90;
            break;

        case -180 :
        case  180 :
            //id = V4L2_CID_ROTATE_180;
            m_angle = 180;
            break;

        case  -90 :
        case  270 :
            //id = V4L2_CID_ROTATE_270;
            m_angle = 270;
            break;

        default :
            LOGE("ERR(%s):Invalid angle(%d)", __FUNCTION__, angle);
            return -1;
    }

    // just set on the Exif of Jpeg
    // m_flag_current_info_changed = FLAG_ON;

    return 0;
}

int SecCamera::getRotate(void)
{
    LOGV("%s():angle(%d)", __FUNCTION__, m_angle);
    return m_angle;
}

void SecCamera::setJpegQuality(int quality)
{
    m_jpeg_quality = quality;
}
// -----------------------------------


void SecCamera::setFrameRate(int frame_rate)
{
    m_frame_rate = frame_rate;
}

int SecCamera::getFrameRate   (void)
{
    return m_frame_rate;
}

int SecCamera::getFrameRateMin(void)
{
    return FRAME_RATE_BASE;
}

int SecCamera::getFrameRateMax(void)
{
    return FRAME_RATE_MAX;
}

// -----------------------------------

int SecCamera::setSceneMode(int scene_mode)
{
    if(scene_mode <= SCENE_MODE_BASE || SCENE_MODE_MAX <= scene_mode) {
        LOGE("%s::invalid Scene mode(%d) it should %d ~ %d\n",
              __func__, scene_mode, SCENE_MODE_BASE, SCENE_MODE_MAX);
        return -1;
    }

    if(m_scene_mode != scene_mode) {
        m_scene_mode = scene_mode;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::setWhiteBalance(int white_balance)
{
    LOGV("%s(white_balance(%d))", __FUNCTION__, white_balance);

    if(white_balance <= WHITE_BALANCE_BASE || WHITE_BALANCE_MAX <= white_balance) {
        LOGE("ERR(%s):Invalid white_balance(%d)", __FUNCTION__, white_balance);
        return -1;
    }

    if(m_white_balance != white_balance) {
        m_white_balance = white_balance;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::getWhiteBalance(void)
{
    LOGV("%s():white_balance(%d)", __FUNCTION__, m_white_balance);
    return m_white_balance;
}

// -----------------------------------

int SecCamera::setImageEffect(int image_effect)
{
    LOGV("%s(image_effect(%d))", __FUNCTION__, image_effect);

    if(image_effect <= IMAGE_EFFECT_BASE || IMAGE_EFFECT_MAX <= image_effect) {
        LOGE("ERR(%s):Invalid image_effect(%d)", __FUNCTION__, image_effect);
        return -1;
    }

    if(m_image_effect != image_effect) {
        m_image_effect = image_effect;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::getImageEffect(void)
{
    LOGV("%s():image_effect(%d)", __FUNCTION__, m_image_effect);
    return m_image_effect;
}

// -----------------------------------

int SecCamera::setBrightness(int brightness)
{
    LOGV("%s(brightness(%d))", __FUNCTION__, brightness);

    if(brightness < BRIGHTNESS_BASE || BRIGHTNESS_MAX < brightness ) {
        LOGE("ERR(%s):Invalid brightness(%d)", __FUNCTION__, brightness);
        return -1;
    }

    if(m_brightness != brightness) {
        m_brightness = brightness;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::getBrightness(void)
{
    LOGV("%s():brightness(%d)", __FUNCTION__, m_brightness);
    return m_brightness;
}


int SecCamera::getBrightnessMin(void)
{
    return BRIGHTNESS_BASE;
}

int SecCamera::getBrightnessMax(void)
{
    return BRIGHTNESS_MAX;
}

// -----------------------------------

int SecCamera::setContrast(int contrast)
{
    if(contrast < CONTRAST_BASE || CONTRAST_MAX < contrast ) {
        LOGE("%s::invalid contrast mode(%d) it should %d ~ %d\n",
              __func__, contrast, CONTRAST_BASE, CONTRAST_MAX);
        return -1;
    }

    if(m_contrast != contrast) {
        m_contrast = contrast;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::getContrast(void)
{
    return m_contrast;
}

int SecCamera::getContrastMin(void)
{
    return CONTRAST_BASE;
}

int SecCamera::getContrastMax(void)
{
    return CONTRAST_MAX;
}

// -----------------------------------

int SecCamera::setSharpness(int sharpness)
{
    if(sharpness < SHARPNESS_BASE || SATURATION_MAX < sharpness) {
        LOGE("%s::invalid Sharpness (%d) it should %d ~ %d\n",
              __func__, sharpness, SATURATION_BASE, SATURATION_MAX);
        return -1;
    }

    if(m_sharpness!= sharpness) {
        m_sharpness = sharpness;
        m_flag_current_info_changed = FLAG_ON;
    }
    return 0;
}

int SecCamera::getSharpness(void)
{
    return m_sharpness;
}

int SecCamera::getSharpnessMin(void)
{
    return SHARPNESS_BASE;
}

int SecCamera::getSharpnessMax(void)
{
    return SHARPNESS_MAX;
}

// -----------------------------------

int SecCamera::setSaturation(int saturation)
{
    if(saturation < SATURATION_BASE || SATURATION_MAX < saturation) {
        LOGE("%s::invalid Saturation (%d) it should %d ~ %d\n",
              __func__, saturation, SATURATION_BASE, SATURATION_MAX);
        return -1;
    }

    if(m_saturation != saturation) {
        m_saturation = saturation;
        m_flag_current_info_changed = FLAG_ON;
    }
    return 0;
}

int SecCamera::getSaturation(void)
{
    return m_saturation;
}

int SecCamera::getSaturationMin(void)
{
    return SATURATION_BASE;
}

int SecCamera::getSaturationMax(void)
{
    return SATURATION_MAX;
}

// -----------------------------------

int SecCamera::setZoom(int zoom)
{
    LOGV("%s()", __FUNCTION__);

    if(zoom < ZOOM_BASE || ZOOM_MAX < zoom) {
        LOGE("%s::invalid zoom (%d) it should %d ~ %d\n",
              __func__, zoom, SATURATION_BASE, SATURATION_MAX);
        return -1;
    }

    if(m_zoom != zoom) {
        m_zoom = zoom;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::getZoom(void)
{
    return m_zoom;
}

int    SecCamera::getZoomMin(void)
{
    return ZOOM_BASE;
}

int    SecCamera::getZoomMax(void)
{
    return ZOOM_MAX;
}

// -----------------------------------

int SecCamera::setAFMode(int af_mode)
{
    LOGV("%s()", __FUNCTION__);

    if(af_mode < AF_MODE_BASE || AF_MODE_MAX < af_mode) {
        LOGE("%s::invalid af_mode (%d) it should %d ~ %d\n",
              __func__, af_mode, AF_MODE_BASE, AF_MODE_MAX);
        return -1;
    }

    if(m_af_mode != af_mode) {
        m_af_mode = af_mode;
        m_flag_current_info_changed = FLAG_ON;
    }

    return 0;
}

int SecCamera::getAFMode(void)
{
    return m_af_mode;
}

int SecCamera::runAF(int flag_on, int * flag_focused)
{
    if(m_flag_create == FLAG_OFF) {
        LOGE("ERR(%s):this is not yet created...\n", __FUNCTION__);
        return -1;
    }

    int ret = 0;

    *flag_focused = 0;

#if 0
    if(   m_af_mode == AF_MODE_AUTO
            || m_af_mode == AF_MODE_MACRO) {
        ret = m_setAF(m_af_mode, FLAG_ON, flag_on, flag_focused);
        if (ret < 0) {
            LOGE("ERR(%s):m_setAF(%d) fail\n", __FUNCTION__, m_af_mode);
            return -1; // autofocus is presumed that always succeed....
        }
    }
    else
#endif
        *flag_focused = 1;

    return 0;
}

// ======================================================================
// Jpeg

unsigned char *SecCamera::getJpeg(unsigned char *snapshot_data, int snapshot_size, int * size)
{
    LOGV("%s()", __FUNCTION__);

    if(m_cam_fd <= 0) {
        LOGE("ERR(%s):Camera was closed\n", __FUNCTION__);
        return NULL;
    }

    unsigned char * jpeg_data = NULL;
    int             jpeg_size = 0;

    jpeg_data = yuv2Jpeg(snapshot_data, snapshot_size, &jpeg_size,
            m_snapshot_width, m_snapshot_height, m_snapshot_v4lformat);

    *size = jpeg_size;
    return jpeg_data;
}

unsigned char *SecCamera::yuv2Jpeg(unsigned char * raw_data, int raw_size,int * jpeg_size,int width, int height, int pixel_format)
{
    LOGI("++%s \n",__FUNCTION__);
#ifdef DUMP_SECJPEG
    FILE        *out_fp;
    char        out_file_name[100];
#endif
    LOGI("%s:raw_data(%p), raw_size(%d), jpeg_size(%d), width(%d), height(%d), format(%d)",
            __FUNCTION__, raw_data, raw_size, *jpeg_size, width, height, pixel_format);

    if(m_jpeg_fd > 0) {
        LOGE_IF(api_jpeg_encode_deinit(m_jpeg_fd) != JPEG_OK,
                "ERR(%s):Fail on api_jpeg_encode_deinit\n", __FUNCTION__);
        m_jpeg_fd = 0;
    }

    m_jpeg_fd = api_jpeg_encode_init();

    LOGV("(%s):JPEG device open ID = %d\n", __FUNCTION__,m_jpeg_fd);
    if(m_jpeg_fd <= 0) {
        m_jpeg_fd = 0;
        LOGE("ERR(%s):Cannot open a jpeg device file\n", __FUNCTION__);
        return NULL;
    }

    if(pixel_format == V4L2_PIX_FMT_RGB565) {
        LOGE("ERR(%s):It doesn't support V4L2_PIX_FMT_RGB565\n", __FUNCTION__);
        return NULL;
    }

    enum jpeg_ret_type    result;

    unsigned char * InBuf = NULL;
    unsigned char * OutBuf = NULL;
    unsigned char * jpeg_data = NULL;
    long            frameSize;
    //exif_file_info_t    ExifInfo;
    struct jpeg_enc_param    enc_param;

    //int input_file_format = JPEG_MODESEL_YCBCR;

    jpeg_stream_format out_file_format = JPEG_422;
    switch(pixel_format) {
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12T:
        case V4L2_PIX_FMT_YUV420:
            out_file_format = JPEG_420;
            break;

        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_YUV422P:
            out_file_format = JPEG_422;
            break;
    }

    LOGI("Step 1: Calculate input size\n");

    if(raw_size == 0) {
        int width, height,frame_size;
        getSnapshotSize(&width, &height, &frame_size);
        if(raw_data == NULL) {
            LOGE("%s %d] Raw data is NULL \n",__func__,__LINE__);
            goto YUV2JPEG_END;
        } else {
            raw_size = frame_size;
        }
    }

    /* get Input buffer address */
    InBuf = (unsigned char*)api_jpeg_get_encode_in_buf(m_jpeg_fd, raw_size);
    if (InBuf == NULL) {
        LOGE("ERR(%s):Fail input buffer is NULL \n", __FUNCTION__);
        goto YUV2JPEG_END;
    }

    LOGI("Step 2: memcpy(InBuf(%p), raw_data(%p), raw_size(%d)", InBuf, raw_data, raw_size);
    memcpy(InBuf, raw_data, raw_size);


    /* get output buffer address */
    OutBuf = (unsigned char*)api_jpeg_get_encode_out_buf(m_jpeg_fd);
    if (OutBuf == NULL) {
        LOGE("output buffer is NULL\n");
        goto YUV2JPEG_END;
    }
    LOGI("Step 3: get OutBuf (%p)\n", OutBuf);

    /* set encode parameters */
    enc_param.width = width;
    enc_param.height = height;
    enc_param.in_fmt = YUV_422; // YCBYCR Only
    enc_param.out_fmt = out_file_format;
    enc_param.quality = QUALITY_LEVEL_2;
    api_jpeg_set_encode_param(&enc_param);

    LOGI("Step 4: excute jpeg encode\n");
    result = api_jpeg_encode_exe(m_jpeg_fd, &enc_param);
    if (result != JPEG_ENCODE_OK) {
        LOGE("encoding failed\n");
        goto YUV2JPEG_END;
    }
    LOGI("Step 5: Done");
    jpeg_data  = OutBuf;
    *jpeg_size = (int)enc_param.size;

#ifdef DUMP_SECJPEG

    //////////////////////////////////////////////////////////////
    // Dump JPEG result file
    //////////////////////////////////////////////////////////////

    sprintf(out_file_name, "/data/%d_%d.jpg",
            width, height);
    LOGI("outFilename : %s width : %d height : %d\n",
            out_file_name, width, height);
    out_fp= fopen(out_file_name, "wb");

    if (out_fp == NULL) {
        LOGE("output file open error\n");
        goto YUV2JPEG_END;
    }

    fwrite(jpeg_data, 1, enc_param.size, out_fp);
    fclose(out_fp);

#endif

YUV2JPEG_END :

    LOGI("--%s \n",__FUNCTION__);
    return jpeg_data;
}

int SecCamera::setJpegThumbnailSize(int width, int height)
{
    LOGI("%s(width(%d), height(%d))", __FUNCTION__, width, height);

    m_jpeg_thumbnail_width    = width;
    m_jpeg_thumbnail_height = height;

    return 0;
}

int SecCamera::getJpegThumbnailSize(int* width, int* height)
{
    if(width)
        *width = m_jpeg_thumbnail_width;
    if(height)
        *height = m_jpeg_thumbnail_height;

    return 0;
}


int SecCamera::setGpsInfo(double latitude, double longitude, unsigned int timestamp, int altitude)
{
    m_gps_latitude  = latitude;
    m_gps_longitude = longitude;
    m_gps_timestamp = timestamp;
    m_gps_altitude  = altitude;

    return 0;
}

int SecCamera::m_resetCamera(void)
{
    int ret = 0;

    if(m_flag_current_info_changed == FLAG_ON) {
        LOGV("m_resetCamera running..");

        // this is already done by preview
        LOGE_IF(m_setCameraId(m_camera_id) < 0,
                "ERR(%s):m_setCameraId fail\n", __FUNCTION__);
        LOGE_IF(m_setZoom(m_zoom) < 0,
                "ERR(%s):m_setZoom fail\n", __FUNCTION__);
        LOGE_IF(m_setFrameRate(m_frame_rate) < 0,
                "ERR(%s):m_setFrameRate fail\n", __FUNCTION__);
        LOGE_IF(m_setSceneMode(m_scene_mode) < 0,
                "ERR(%s):m_setSceneMode fail\n", __FUNCTION__);
        LOGE_IF(m_setWhiteBalance(m_white_balance) < 0,
                "ERR(%s):m_setWhiteBalance fail\n", __FUNCTION__);
        LOGE_IF(m_setImageEffect(m_image_effect) < 0,
                "ERR(%s):m_setImageEffect fail\n", __FUNCTION__);
        LOGE_IF(m_setBrightness(m_brightness) < 0,
                "ERR(%s):m_setBrightness fail\n", __FUNCTION__);
        LOGE_IF(m_setContrast(m_contrast) < 0,
                "ERR(%s):m_setContrast fail\n", __FUNCTION__);
        LOGE_IF(m_setSharpness(m_sharpness) < 0,
                "ERR(%s):m_setSharpness fail\n", __FUNCTION__);
        LOGE_IF(m_setSaturation(m_saturation) < 0,
                "ERR(%s):m_setSaturation fail\n", __FUNCTION__);

        int flag_focused = 0;
        LOGE_IF(m_setAF(m_af_mode, FLAG_OFF, FLAG_OFF, &flag_focused) < 0,
                "ERR(%s):m_setAF fail\n", __FUNCTION__);

        m_flag_current_info_changed = FLAG_OFF;
    }

    return ret;
}

int SecCamera::m_setCameraId(int camera_id)
{
    if(camera_id == m_current_camera_id)
        return 0;

    LOGV("%s(camera_id(%d))", __FUNCTION__, camera_id);

    switch(camera_id) {
        case CAMERA_ID_FRONT:
            m_preview_max_width   = MAX_FRONT_CAMERA_PREVIEW_WIDTH;
            m_preview_max_height  = MAX_FRONT_CAMERA_PREVIEW_HEIGHT;
            m_snapshot_max_width  = MAX_FRONT_CAMERA_SNAPSHOT_WIDTH;
            m_snapshot_max_height = MAX_FRONT_CAMERA_SNAPSHOT_HEIGHT;
            break;

        case CAMERA_ID_BACK:
            m_preview_max_width   = MAX_BACK_CAMERA_PREVIEW_WIDTH;
            m_preview_max_height  = MAX_BACK_CAMERA_PREVIEW_HEIGHT;
            m_snapshot_max_width  = MAX_BACK_CAMERA_SNAPSHOT_WIDTH;
            m_snapshot_max_height = MAX_BACK_CAMERA_SNAPSHOT_HEIGHT;
            break;

        default:
            LOGE("ERR(%s)::Invalid camera id(%d)\n", __func__, camera_id);
            return -1;
    }

    m_current_camera_id = camera_id;

    return 0;
}

int SecCamera::m_setFrameRate(int frame_rate)
{
    if(frame_rate == m_current_frame_rate)
        return 0;

    LOGV("%s(frame_rate(%d))", __FUNCTION__, frame_rate);

    int ret = 0;

    /* g_parm, s_parm sample */
    ret = fimc_v4l2_g_parm(m_cam_fd);
    CHECK(ret);
    ret = fimc_v4l2_s_parm(m_cam_fd, 1, frame_rate);
    CHECK(ret);

    m_current_frame_rate = frame_rate;

    return 0;
}

int SecCamera::m_setSceneMode(int scene_mode)
{
    if(scene_mode == m_current_scene_mode)
        return 0;

    LOGV("%s(scene_mode(%d))", __FUNCTION__, scene_mode);

#if 0
    int value = 0;

    switch (scene_mode) {
        case SCENE_MODE_AUTO:
            // Always load the UI when you are coming back to auto mode
            m_current_white_balance = -1;
            m_current_brightness    = -1;
            m_current_contrast      = -1;
            m_current_saturation    = -1;
            m_current_sharpness     = -1;
            m_current_metering      = -1;
            m_current_iso           = -1;
            m_current_image_effect  = -1;
            value = 0;
            break;
        case SCENE_MODE_BEACH:
            value = 9;
            break;
        case SCENE_MODE_BACKLIGHT:
            value = 11;
            break;
        case SCENE_MODE_CANDLELIGHT:
            value = 5;
            break;
        case SCENE_MODE_DUSKDAWN:
            value = 12;
            break;
        case SCENE_MODE_FALLCOLOR:
            value = 13;
            break;
        case SCENE_MODE_FIREWORKS:
            value = 6;
            break;
        case SCENE_MODE_LANDSCAPE:
            value = 2;
            break;
        case SCENE_MODE_NIGHT:
            value = 8;
            break;
        case SCENE_MODE_PARTY:
            value = 10;
            break;
        case SCENE_MODE_PORTRAIT:
            value = 1;
            break;
        case SCENE_MODE_SNOW:
            value = 9;
            break;
        case SCENE_MODE_SPORTS:
            value = 3;
            break;
        case SCENE_MODE_SUNSET:
            value = 4;
            break;
        case SCENE_MODE_TEXT:
            value = 7;
            break;
        default :
            LOGE("%s::invalid scene_mode : %d\n", __func__, scene_mode);
            return -1;
            break;
    }

    if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_SCENE_MODE, value)){
        LOGE("ERR(%s):V4L2_CID_SCENE_MODE(%d) fail\n", __FUNCTION__, scene_mode);
        return -1;
    }
#endif

    m_current_scene_mode = scene_mode;

    return 0;
}


int SecCamera::m_setWhiteBalance(int white_balance)
{
    if(white_balance == m_current_white_balance)
        return 0;

    LOGV("%s(white_balance(%d))", __FUNCTION__, white_balance);

    int value = 0;

    if(white_balance == WHITE_BALANCE_AUTO) {
        value = 1;

        if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_AUTO_WHITE_BALANCE, value) < 0) {
            LOGE("ERR(%s):V4L2_CID_AUTO_WHITE_BALANCE(%d) fail\n", __FUNCTION__, white_balance);
            return -1;
        }
    } else {
        switch (white_balance) {
            //case WHITE_BALANCE_AUTO:
            //    value = 1;
            //    break;
            case WHITE_BALANCE_CLOUDY:
                value = 3;
                break;
            case WHITE_BALANCE_SUNNY:
                value = 2;
                break;
            case WHITE_BALANCE_FLUORESCENT:
                value = 1;
                break;
            case WHITE_BALANCE_INCANDESCENT:
                value = 0;
                break;
            default :
                LOGE("%s::invalid white_balance : %d\n", __func__, white_balance);
                return -1;
                break;
        }

        // this is very temporary enum..
#if 0
#define V4L2_CID_WHITE_BALANCE_PRESET   (V4L2_CID_CAMERA_CLASS_BASE+27)

        if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_WHITE_BALANCE_PRESET, value) < 0) {
            LOGE("ERR(%s):V4L2_CID_WHITE_BALANCE_PRESET(%d) fail\n", __FUNCTION__, white_balance);
            return -1;
        }
#endif
    }

    m_current_white_balance = white_balance;

    return 0;
}


int SecCamera::m_setImageEffect(int image_effect)
{
    if(image_effect == m_current_image_effect)
        return 0;

    LOGV("%s(image_effect(%d))", __FUNCTION__, image_effect);

    int value = 0;

    switch (image_effect) {
        case IMAGE_EFFECT_ORIGINAL:
            value = 0;
            break;
        case IMAGE_EFFECT_AQUA:
            value = 4;
            break;
        case IMAGE_EFFECT_MONO:
            value = 1;
            break;
        case IMAGE_EFFECT_NEGATIVE:
            value = 2;
            break;
        case IMAGE_EFFECT_SEPIA:
            value = 3;
            break;
        case IMAGE_EFFECT_WHITEBOARD:
            value = 5;
            break;
        default :
            LOGE("%s::invalid image_effect : %d\n", __func__, image_effect);
            return -1;
            break;
    }

    if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_COLORFX, value) < 0) {
        LOGE("ERR(%s):V4L2_CID_COLORFX(%d) fail\n", __FUNCTION__, image_effect);
        return -1;
    }

    m_current_image_effect = image_effect;

    return 0;
}


int SecCamera::m_setBrightness(int brightness)
{
    if(brightness == m_current_brightness)
        return 0;

    LOGV("%s(brightness(%d))", __FUNCTION__, brightness);

    if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_EXPOSURE, brightness) < 0) {
        LOGE("ERR(%s):V4L2_CID_EXPOSURE(%d) fail\n", __FUNCTION__, brightness);
        return -1;
    }

    m_current_brightness = brightness;

    return 0;
}


int SecCamera::m_setContrast(int contrast)
{
    if(contrast == m_current_contrast)
        return 0;

    LOGV("%s(contrast(%d))", __FUNCTION__, contrast);

    if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CONTRAST, contrast) < 0) {
        LOGE("ERR(%s):V4L2_CID_CONTRAST(%d) fail\n", __FUNCTION__, contrast);
        return -1;
    }

    m_current_contrast = contrast;

    return 0;
}

//======================================================================
int SecCamera::setRecordingSize(int width, int height)
{
     LOGV("%s(width(%d), height(%d))", __func__, width, height);

     m_recording_width  = width;
     m_recording_height = height;

     return 0;
}

int SecCamera::m_setSharpness(int sharpness)
{
    if(sharpness == m_current_sharpness)
        return 0;

    LOGV("%s(sharpness(%d))", __FUNCTION__, sharpness);

    if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_SHARPNESS, sharpness) < 0) {
        LOGE("ERR(%s):V4L2_CID_SHARPNESS(%d) fail\n", __FUNCTION__, sharpness);
        return -1;
    }

    m_current_sharpness = sharpness;

    return 0;
}

int  SecCamera::m_setSaturation(int saturation)
{
    if(saturation == m_current_saturation)
        return 0;

    LOGV("%s(saturation(%d))", __FUNCTION__, saturation);

    if(fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_SATURATION, saturation) < 0) {
        LOGE("ERR(%s):V4L2_CID_CONTRAST(%d) fail\n", __FUNCTION__, saturation);
        return -1;
    }

    m_current_saturation = saturation;

    return 0;
}

int SecCamera::m_setZoom(int zoom)
{
    if(zoom == m_current_zoom)
        return 0;

    LOGV("%s(zoom(%d))", __FUNCTION__, zoom);

#ifdef USE_SEC_CROP_ZOOM
    // set crop
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    unsigned int crop_x      = 0;
    unsigned int crop_y      = 0;
    unsigned int crop_width  = 0;
    unsigned int crop_height = 0;

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(m_cam_fd, VIDIOC_CROPCAP, &cropcap) >= 0) {
        m_getCropRect(cropcap.bounds.width, cropcap.bounds.height,
                m_preview_width,     m_preview_height,
                &crop_x,             &crop_y,
                &crop_width,         &crop_height,
                m_zoom);

        if(   (unsigned int)cropcap.bounds.width  != crop_width
                || (unsigned int)cropcap.bounds.height != crop_height) {
            cropcap.defrect.left   = crop_x;
            cropcap.defrect.top    = crop_y;
            cropcap.defrect.width  = crop_width;
            cropcap.defrect.height = crop_height;
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c    = cropcap.defrect;

            if (ioctl(m_cam_fd, VIDIOC_S_CROP, &crop) < 0)
                LOGE("%s(VIDIOC_S_CROP fail(%d))", __FUNCTION__, zoom);
        }

        /*
           LOGV("## 1 cropcap.bounds.width  : %d \n", cropcap.bounds.width);
           LOGV("## 1 cropcap.bounds.height : %d \n", cropcap.bounds.height);
           LOGV("## 1 width                 : %d \n", width);
           LOGV("## 1 height                : %d \n", height);
           LOGV("## 1 m_zoom                : %d \n", m_zoom);
           LOGV("## 2 crop_width            : %d \n", crop_width);
           LOGV("## 2 crop_height           : %d \n", crop_height);
           LOGV("## 2 cropcap.defrect.width : %d \n", cropcap.defrect.width);
           LOGV("## 2 cropcap.defrect.height: %d \n", cropcap.defrect.height);
         */
    }
    else
        LOGE("%s(VIDIOC_CROPCAP fail (bug ignored..))", __FUNCTION__);
    // ignore errors

#ifdef DUAL_PORT_RECORDING
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(m_cam_fd2, VIDIOC_CROPCAP, &cropcap) >= 0) {
        m_getCropRect(cropcap.bounds.width, cropcap.bounds.height,
                m_preview_width,     m_preview_height,
                &crop_x,             &crop_y,
                &crop_width,         &crop_height,
                m_zoom);

        if(   (unsigned int)cropcap.bounds.width  != crop_width
                || (unsigned int)cropcap.bounds.height != crop_height) {
            cropcap.defrect.left   = crop_x;
            cropcap.defrect.top    = crop_y;
            cropcap.defrect.width  = crop_width;
            cropcap.defrect.height = crop_height;
            crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            crop.c    = cropcap.defrect;

            if (ioctl(m_cam_fd2, VIDIOC_S_CROP, &crop) < 0)
                LOGE("%s(VIDIOC_S_CROP fail(%d))", __FUNCTION__, zoom);
        }

        /*
           LOGV("## 1 cropcap.bounds.width  : %d \n", cropcap.bounds.width);
           LOGV("## 1 cropcap.bounds.height : %d \n", cropcap.bounds.height);
           LOGV("## 1 width                 : %d \n", width);
           LOGV("## 1 height                : %d \n", height);
           LOGV("## 1 m_zoom                : %d \n", m_zoom);
           LOGV("## 2 crop_width            : %d \n", crop_width);
           LOGV("## 2 crop_height           : %d \n", crop_height);
           LOGV("## 2 cropcap.defrect.width : %d \n", cropcap.defrect.width);
           LOGV("## 2 cropcap.defrect.height: %d \n", cropcap.defrect.height);
         */
    } else
        LOGE("%s(VIDIOC_CROPCAP fail (bug ignored..))", __FUNCTION__);
#endif
#endif // USE_SEC_CROP_ZOOM

    m_current_zoom = zoom;

    return 0;
}


int SecCamera::m_setAF(int af_mode, int flag_run, int flag_on, int * flag_focused)
{
    int ret = -1;

    *flag_focused = 0;
    static int autofocus_running = FLAG_OFF;

    // setting af mode
    if(m_current_af_mode != af_mode
            && autofocus_running == FLAG_OFF) {
        LOGV("%s called::for setting af_mode : %d \n", __func__, af_mode);

        autofocus_running = FLAG_ON;

#if 0
        struct v4l2_control ctrl;
        ctrl.id    = V4L2_CID_FOCUS_AUTO;

        switch(af_mode) {
            case AF_MODE_AUTO :
                ctrl.value = 2;
                break;
            case AF_MODE_FIXED :
                ctrl.value = 5;
                break;
            case AF_MODE_INFINITY :
                ctrl.value = 3;
                break;
            case AF_MODE_MACRO :
                ctrl.value = 4;
                break;
            default :
                LOGE("%s(unmatched af mode(%d) fail)", __FUNCTION__, af_mode);
                goto m_setAF_end;
                break;
        }
        ret = ioctl(m_camera_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret < 0) {
            LOGE("%s(VIDIOC_S_FOCUS_AUTO(%d) fail)", __FUNCTION__, flag_on);
            goto m_setAF_end; // autofocus is presumed that always succeed....
        }
        else if (ret == 1)
            *flag_focused = 1;
        //else
        //    *flag_focused = 0;
#else
        *flag_focused = 1;
#endif

        m_current_af_mode = af_mode;

        autofocus_running = FLAG_OFF;
    }

    // running af mode
    if(   (flag_run == FLAG_ON)
            && (af_mode == AF_MODE_AUTO || af_mode == AF_MODE_MACRO)
            && (autofocus_running == FLAG_OFF)) {
        LOGV("%s called::for running af_mode : %d \n", __func__, af_mode);

        autofocus_running = FLAG_ON;

#if 1
        for(int i = 0; i < 10 ; i++) {
            LOGV("VIRTUAL AUTOFOCUSING IS ON %d...\n", i);
            usleep(100000);
        }

        *flag_focused = 1;
#else
        struct v4l2_control ctrl;
        ctrl.id    = V4L2_CID_FOCUS_AUTO;

        if(flag_on == FLAG_ON)
            ctrl.value = 1;
        else // if(flag_on == FLAG_OFF) {
#ifdef AF_RELEASE_FUNCTION_ENABLE
            ctrl.value = 0;
#else
        ret = 0;
        goto m_setAF_end;
#endif // AF_RELEASE_FUNCTION_ENABLE
    }

    ret = ioctl(m_camera_fd, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        LOGE("%s(VIDIOC_S_FOCUS_AUTO(%d) fail)", __FUNCTION__, flag_on);
        goto m_setAF_end; // autofocus is presumed that always succeed....
    } else if (ret == 1)
        *flag_focused = 1;
    //else
    //    *flag_focused = 0;
#endif
    autofocus_running = FLAG_OFF;
    } else {
        // just set focused..
        *flag_focused = 1;
    }

    ret = 0;

m_setAF_end :

    autofocus_running = FLAG_OFF;

    return ret;
}

int SecCamera::m_getCropRect(unsigned int   src_width,  unsigned int   src_height,
                             unsigned int   dst_width,  unsigned int   dst_height,
                             unsigned int * crop_x,     unsigned int * crop_y,
                             unsigned int * crop_width, unsigned int * crop_height,
                             int            zoom)
{
#define DEFAULT_ZOOM_RATIO        (4) // 4x zoom
#define DEFAULT_ZOOM_RATIO_SHIFT  (2)

    unsigned int cal_src_width   = src_width;
    unsigned int cal_src_height  = src_height;

    if(   zoom != 0
            || src_width  != dst_width
            || src_height != dst_height) {
        float src_ratio    = 1.0f;
        float dst_ratio    = 1.0f;

        // ex : 1024 / 768
        src_ratio  = (float)src_width / (float)src_height ;

        // ex : 352  / 288
        dst_ratio =  (float)dst_width / (float)dst_height;

        if(src_ratio != dst_ratio) {
            if(src_ratio <= dst_ratio) {
                cal_src_width  = src_width;
                cal_src_height = src_width / dst_ratio;
            } else { //(src_ratio > dst_ratio)
                cal_src_width  = src_height * dst_ratio;
                cal_src_height = src_height;
            }
        }

        if(zoom != 0) {
            unsigned int zoom_width_step =
                (src_width  - (src_width  >> DEFAULT_ZOOM_RATIO_SHIFT)) / ZOOM_MAX;

            unsigned int zoom_height_step =
                (src_height - (src_height >> DEFAULT_ZOOM_RATIO_SHIFT)) / ZOOM_MAX;


            cal_src_width  = cal_src_width   - (zoom_width_step  * zoom);
            cal_src_height = cal_src_height  - (zoom_height_step * zoom);
        }
    }

#if 0
#define CAMERA_CROP_RESTRAIN_NUM  (0x10)
    unsigned int width_align = (cal_src_width & (CAMERA_CROP_RESTRAIN_NUM-1));
    if(width_align != 0) {
        if(    (CAMERA_CROP_RESTRAIN_NUM >> 1) <= width_align
                && cal_src_width + (CAMERA_CROP_RESTRAIN_NUM - width_align) <= dst_width) {
            cal_src_width += (CAMERA_CROP_RESTRAIN_NUM - width_align);
        } else
            cal_src_width -= width_align;
    }
#endif
    // kcoolsw : this can be camera view weird..
    //           because dd calibrate x y once again
    *crop_x      = (src_width  - cal_src_width ) >> 1;
    *crop_y      = (src_height - cal_src_height) >> 1;
    //*crop_x      = 0;
    //*crop_y      = 0;
    *crop_width  = cal_src_width;
    *crop_height = cal_src_height;

    return 0;
}
// ======================================================================
// Conversions

inline unsigned int SecCamera::m_frameSize(int format, int width, int height)
{
    unsigned int size = 0;
    unsigned int realWidth  = width;
    unsigned int realheight = height;

    switch(format) {
    case V4L2_PIX_FMT_YUV420 :
    case V4L2_PIX_FMT_NV12 :
    case V4L2_PIX_FMT_NV21 :
        size = ((realWidth * realheight * 3) >> 1);
        break;

    case V4L2_PIX_FMT_NV12T:
        size =   ALIGN_TO_8KB(ALIGN_TO_128B(realWidth) * ALIGN_TO_32B(realheight))
            + ALIGN_TO_8KB(ALIGN_TO_128B(realWidth) * ALIGN_TO_32B(realheight >> 1));
        break;

    case V4L2_PIX_FMT_YUV422P :
    case V4L2_PIX_FMT_YUYV :
    case V4L2_PIX_FMT_UYVY :
        size = ((realWidth * realheight) << 1);
        break;

    default :
        LOGE("ERR(%s):Invalid V4L2 pixel format(%d)\n", __FUNCTION__, format);
    case V4L2_PIX_FMT_RGB565 :
        size = (realWidth * realheight * BPP);
        break;
    }

    return size;
}

status_t SecCamera::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    snprintf(buffer, 255, "dump(%d)\n", fd);
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return NO_ERROR;
}


}; // namespace android
