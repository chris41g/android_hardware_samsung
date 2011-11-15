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

//#define LOG_NDEBUG 0
#define LOG_TAG "libhdmi"
#include <cutils/log.h>

#include "SecHdmi.h"
#include "fimd_api.h"
#if defined(BOARD_USES_FIMGAPI)
#include "FimgApi.h"
#endif


namespace android {

unsigned int output_type = V4L2_OUTPUT_TYPE_DIGITAL;
v4l2_std_id t_std_id     = V4L2_STD_1080P_30;
static int g_hpd_state   = HPD_CABLE_OUT;
static unsigned int g_hdcp_en = 0;

static int fp_tvout    = -1;
static int fp_tvout_v  = -1;
static int fp_tvout_g0 = -1;
static int fp_tvout_g1 = -1;

struct vid_overlay_param vo_param;

#if defined(BOARD_USES_FIMGAPI)
static unsigned int g2d_reserved_memory0     = 0;
static unsigned int g2d_reserved_memory1     = 0;
static unsigned int g2d_reserved_memory_size = 0;
#endif

void display_menu(void)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    //LOGI("=============== HDMI AVMUTE =============\n");
    //LOGI("=  x. mute                              =\n");
    //LOGI("=  o. continue                          =\n");
    LOGI("=============== HDMI Audio  =============\n");

    if (EDIDAudioModeSupport(&audio))
        LOGI("=  2CH_PCM 44100Hz audio supported      =\n");

    LOGI("========= HDMI Mode & Color Space =======\n");

    video.mode = HDMI;
    if (EDIDHDMIModeSupport(&video))
    {
        video.colorSpace = HDMI_CS_YCBCR444;
        if (EDIDColorSpaceSupport(&video))
            LOGI("=  1. HDMI(YCbCr)                       =\n");

        video.colorSpace = HDMI_CS_RGB;
        if (EDIDColorSpaceSupport(&video))
            LOGI("=  2. HDMI(RGB)                         =\n");
    }
    else
    {
        video.mode = DVI;
        if (EDIDHDMIModeSupport(&video)) {
            LOGI("=  3. DVI                               =\n");
        }
    }

    LOGI("===========    HDMI Rseolution   ========\n");

    /* 480P */
    video.resolution = v720x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9 ;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  4. 480P_60_16_9 	(0x04000000)	=\n");

    video.resolution = v640x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  5. 480P_60_4_3	(0x05000000)	=\n");


    /* 576P */
    video.resolution = v720x576p_50Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  6. 576P_50_16_9 	(0x06000000)	=\n");

    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  7. 576P_50_4_3	(0x07000000)	=\n");

    /* 720P 60 */
    video.resolution = v1280x720p_60Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  8. 720P_60 		(0x08000000)	=\n");

    /* 720P_50 */
    video.resolution = v1280x720p_50Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  9. 720P_50 		(0x09000000)	=\n");

    /* 1080P_60 */
    video.resolution = v1920x1080p_60Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  a. 1080P_60 		(0x0a000000)	=\n");

    /* 1080P_50 */
    video.resolution = v1920x1080p_50Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  b. 1080P_50 		(0x0b000000)	=\n");

    /* 1080I_60 */
    video.resolution = v1920x1080i_60Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  c. 1080I_60		(0x0c000000)	=\n");

    /* 1080I_50 */
    video.resolution = v1920x1080i_50Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  d. 1080I_50		(0x0d000000)	=\n");

    /* 1080P_30 */
    video.resolution = v1920x1080p_30Hz;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  i. 1080P_30 		(0x12000000)	=\n");

    LOGI("=========================================\n");
}

int tvout_open(const char *fp_name)
{
    int fp;

    fp = open(fp_name, O_RDWR);
    if (fp < 0) {
        LOGE("drv (%s) open failed!!\n", fp_name);
    }

    return fp;
}


int tvout_init(v4l2_std_id std_id)
{
    //unsigned int fp_tvout;
    int ret;
    struct v4l2_output output;
    struct v4l2_standard std;
    v4l2_std_id std_g_id;
    struct tvout_param tv_g_param;

    unsigned int matched=0, i=0;
    int output_index;

    // It was initialized already
    if(fp_tvout <= 0)
    {
        fp_tvout = tvout_open(TVOUT_DEV);
        if (fp_tvout < 0) {
            LOGE("tvout video drv open failed\n");
            return -1;
        }
    }

    if( output_type >= V4L2_OUTPUT_TYPE_DIGITAL &&
        output_type <= V4L2_OUTPUT_TYPE_DVI)
        ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, g_hdcp_en);

    /* ============== query capability============== */
    tvout_v4l2_querycap(fp_tvout);

    tvout_v4l2_enum_std(fp_tvout, &std, std_id);

    // set std
    tvout_v4l2_s_std(fp_tvout, std_id);
    tvout_v4l2_g_std(fp_tvout, &std_g_id);

    i = 0;

    do {
        output.index = i;
        ret = tvout_v4l2_enum_output(fp_tvout, &output);
        if(output.type == output_type)
        {
            matched = 1;
            break;
        }
        i++;
    } while(ret >=0);

    if(!matched)
    {
        LOGE("no matched output type [type : 0x%08x]\n", output_type);
        return -1;
    }

    // set output
    tvout_v4l2_s_output(fp_tvout, output.index);
    output_index = 0;
    tvout_v4l2_g_output(fp_tvout, &output_index);


    //set fmt param
    vo_param.src.base_y         = (void *)0x0;
    vo_param.src.base_c         = (void *)0x0;
    vo_param.src.pix_fmt.width  = 0;
    vo_param.src.pix_fmt.height = 0;
    vo_param.src.pix_fmt.field  = V4L2_FIELD_NONE;
    //tv_param.tvout_src.pix_fmt.pixelformat = VPROC_SRC_COLOR_TILE_NV12;
    vo_param.src.pix_fmt.pixelformat = V4L2_PIX_FMT_NV12T;

    vo_param.src_crop.left    = 0;
    vo_param.src_crop.top     = 0;
    vo_param.src_crop.width   = 0;
    vo_param.src_crop.height  = 0;

    return fp_tvout;
}

int tvout_deinit()
{
    if(0 < fp_tvout)
    {
        close(fp_tvout);
        fp_tvout = -1;
    }
    return 0;
}

int tvout_v4l2_querycap(int fp)
{
    struct v4l2_capability cap;
    int ret;

    ret = ioctl(fp, VIDIOC_QUERYCAP, &cap);

    if (ret < 0) {
        LOGE("tvout_v4l2_querycap" "VIDIOC_QUERYCAP failed %d\n", errno);
        return ret;
    }

    //if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
    //	LOGE("no overlay devices\n");
    //	return ret;
    //}

    LOGV("tvout_v4l2_querycap" "DRIVER : %s, CARD : %s, CAP.: 0x%08x\n",
            cap.driver, cap.card, cap.capabilities);

    return ret;
}

/*
   ioctl VIDIOC_G_STD, VIDIOC_S_STD
   To query and select the current video standard applications use the VIDIOC_G_STD and
   VIDIOC_S_STD ioctls which take a pointer to a v4l2_std_id type as argument. VIDIOC_G_STD can
   return a single flag or a set of flags as in struct v4l2_standard field id
   */

int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id)
{
    int ret;

    ret = ioctl(fp, VIDIOC_G_STD, std_id);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_std" "VIDIOC_G_STD failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_v4l2_s_std(int fp, v4l2_std_id std_id)
{
    int ret;

    ret = ioctl(fp, VIDIOC_S_STD, &std_id);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_std" "VIDIOC_S_STD failed %d\n", errno);
        return ret;
    }

    return ret;
}

/*
   ioctl VIDIOC_ENUMSTD
   To query the attributes of a video standard, especially a custom (driver defined) one, applications
   initialize the index field of struct v4l2_standard and call the VIDIOC_ENUMSTD ioctl with a pointer
   to this structure. Drivers fill the rest of the structure or return an EINVAL error code when the index
   is out of bounds.
   */
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id)
{
    std->index = 0;
    while (0 == ioctl (fp, VIDIOC_ENUMSTD, std))
    {
        if (std->id & std_id)
        {
            LOGV("tvout_v4l2_enum_std" "Current video standard: %s\n", std->name);
        }
        std->index++;
    }

    return 0;
}

/*
   ioctl VIDIOC_ENUMOUTPUT
   To query the attributes of a video outputs applications initialize the index field of struct v4l2_output
   and call the VIDIOC_ENUMOUTPUT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code when the index is out of bounds
   */
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output)
{
    int ret;

    ret = ioctl(fp, VIDIOC_ENUMOUTPUT, output);

    if(ret >=0){
        LOGV("tvout_v4l2_enum_output" "enum. output [index = %d] :: type : 0x%08x , name = %s\n",
                output->index,output->type,output->name);
    }

    return ret;
}

/*
   ioctl VIDIOC_G_OUTPUT, VIDIOC_S_OUTPUT
   To query the current video output applications call the VIDIOC_G_OUTPUT ioctl with a pointer to an
   integer where the driver stores the number of the output, as in the struct v4l2_output index field.
   This ioctl will fail only when there are no video outputs, returning the EINVAL error code
   */
int tvout_v4l2_s_output(int fp, int index)
{
    int ret;

    ret = ioctl(fp, VIDIOC_S_OUTPUT, &index);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_output" "VIDIOC_S_OUTPUT failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_v4l2_g_output(int fp, int *index)
{
    int ret;

    ret = ioctl(fp, VIDIOC_G_OUTPUT, index);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_output" "VIDIOC_G_OUTPUT failed %d\n", errno);
        return ret;
    }else{
        LOGV("tvout_v4l2_g_output" "Current output index %d\n", *index);
    }

    return ret;
}

/*
   ioctl VIDIOC_ENUM_FMT
   To enumerate image formats applications initialize the type and index field of struct v4l2_fmtdesc
   and call the VIDIOC_ENUM_FMT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code. All formats are enumerable by beginning at index zero
   and incrementing by one until EINVAL is returned.
   */
int tvout_v4l2_enum_fmt(int fp, struct v4l2_fmtdesc *desc)
{
    desc->index = 0;
    while (0 == ioctl(fp, VIDIOC_ENUM_FMT, desc))
    {
        LOGV("tvout_v4l2_enum_fmt" "enum. fmt [id : 0x%08x] :: type = 0x%08x, name = %s, pxlfmt = 0x%08x\n",
                desc->index,
                desc->type,
                desc->description,
                desc->pixelformat);
        desc->index++;
    }

    return 0;
}

int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr)
{
    int ret;
    struct v4l2_format format;
    struct v4l2_pix_format_s5p_tvout *fmt_param = (struct v4l2_pix_format_s5p_tvout*)ptr;

    format.type = (enum v4l2_buf_type)buf_type;

    ret = ioctl(fp, VIDIOC_G_FMT, &format);
    if (ret < 0)
    {
        LOGE("tvout_v4l2_g_fmt" "type : %d, VIDIOC_G_FMT failed %d\n", buf_type, errno);
        return ret;
    }
    else
    {
        memcpy(fmt_param, format.fmt.raw_data, sizeof(struct v4l2_pix_format_s5p_tvout));
        LOGV("tvout_v4l2_g_fmt" "get. fmt [base_c : 0x%08x], [base_y : 0x%08x] type = 0x%08x, width = %d, height = %d\n",
                fmt_param->base_c,
                fmt_param->base_y,
                fmt_param->pix_fmt.pixelformat,
                fmt_param->pix_fmt.width,
                fmt_param->pix_fmt.height);
    }

    return 0;
}

int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr)
{
    struct v4l2_format format;
    int ret;

    format.type = (enum v4l2_buf_type)buf_type;
    switch (buf_type) {
    case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        format.fmt.win =  *((struct v4l2_window *) ptr);
        break;

    case V4L2_BUF_TYPE_PRIVATE: {
        struct v4l2_vid_overlay_src *fmt_param =
            (struct v4l2_vid_overlay_src *) ptr;

        memcpy(format.fmt.raw_data, fmt_param,
                sizeof(struct v4l2_vid_overlay_src));
        break;
    }

    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    {
        struct v4l2_pix_format_s5p_tvout *fmt_param =
            (struct v4l2_pix_format_s5p_tvout *)ptr;
        memcpy(format.fmt.raw_data, fmt_param,
                sizeof(struct v4l2_pix_format_s5p_tvout));
        break;
    }
    default:
        break;
    }

    ret = ioctl(fp, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_fmt [tvout_v4l2_s_fmt] : type : %d, VIDIOC_S_FMT failed %d\n",
                buf_type, errno);
        return ret;
    }
    return 0;

}

int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame)
{
    int ret;

    ret = ioctl(fp, VIDIOC_G_FBUF, frame);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
        return ret;
    }

    LOGV("tvout_v4l2_g_fbuf" "get. fbuf: base = 0x%08X, pixel format = %d\n",
            frame->base,
            frame->fmt.pixelformat);
    return 0;
}

int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame)
{
    int ret;

    ret = ioctl(fp, VIDIOC_S_FBUF, frame);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
        return ret;
    }
    return 0;
}

int tvout_v4l2_s_baseaddr(int fp, void *base_addr)
{
    int ret;

    ret = ioctl(fp, S5PTVFB_WIN_SET_ADDR, base_addr);
    if (ret < 0) {
        LOGE("tvout_v4l2_baseaddr" "VIDIOC_S_BASEADDR failed %d\n", errno);
        return ret;
    }
    return 0;
}

int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
    int ret;
    struct v4l2_crop crop;
    crop.type = (enum v4l2_buf_type)type;
    ret = ioctl(fp, VIDIOC_G_CROP, &crop);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_crop" "VIDIOC_G_CROP failed %d\n", errno);
        return ret;
    }

    rect->left	= crop.c.left;
    rect->top	= crop.c.top;
    rect->width	= crop.c.width;
    rect->height	= crop.c.height;

    LOGV("tvout_v4l2_g_crop" "get. crop : left = %d, top = %d, width  = %d, height = %d\n",
            rect->left,
            rect->top,
            rect->width,
            rect->height);
    return 0;
}

int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
    struct v4l2_crop crop;
    int ret;

    crop.type 	= (enum v4l2_buf_type)type;

    crop.c.left 	= rect->left;
    crop.c.top 	    = rect->top;
    crop.c.width 	= rect->width;
    crop.c.height 	= rect->height;

    ret = ioctl(fp, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_crop" "VIDIOC_S_CROP failed %d\n", errno);
        return ret;
    }

    return 0;
}

int tvout_v4l2_start_overlay(int fp)
{
    int ret, start = 1;

    ret = ioctl(fp, VIDIOC_OVERLAY, &start);
    if (ret < 0) {
        LOGE("tvout_v4l2_start_overlay" "VIDIOC_OVERLAY failed\n");
        return ret;
    }

    return ret;
}

int tvout_v4l2_stop_overlay(int fp)
{
    int ret, stop =0;

    ret = ioctl(fp, VIDIOC_OVERLAY, &stop);
    if (ret < 0)
    {
        LOGE("tvout_v4l2_stop_overlay" "VIDIOC_OVERLAY failed\n");
        return ret;
    }

    return ret;
}

///////////////////////////////////////////////////////

int hdmi_init_layer(int layer)
{
    int ret = 0;
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s (layer = %d) called\n", __func__, layer);
#endif

    switch(layer)
    {
    case HDMI_LAYER_VIDEO :
    {
        if(fp_tvout_v <= 0)
        {
            fp_tvout_v = tvout_open(TVOUT_DEV_V);
            if (fp_tvout_v < 0) {
                LOGE("tvout video drv open failed\n");
                return -1;
            }
        }
        break;
    }
    case HDMI_LAYER_GRAPHIC_0 :
        {
            if(fp_tvout_g0 <= 0)
            {
                fp_tvout_g0 	= fb_open(TVOUT_FB_G0);
            }
            break;
        }
    case HDMI_LAYER_GRAPHIC_1 :
        {
            if(fp_tvout_g1 <= 0)
            {
                fp_tvout_g1 	= fb_open(TVOUT_FB_G1);
            }
            break;
        }
    default :
    {

        LOGE("%s::unmathced layer(%d) fail", __func__, layer);
        ret = -1;
        break;
    }
    }

    return ret;
}


int hdmi_deinit_layer(int layer)
{
    int ret = 0;
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s(layer = %d) called\n", __func__, layer);
#endif
    switch(layer)
    {
    case HDMI_LAYER_VIDEO :
        {
            // sw5771.park(101005) :
            // If close on running time, when open, open has lock-up..
            if(0 < fp_tvout_v)
            {
                close(fp_tvout_v);
                fp_tvout_v = -1;
            }
            break;
        }
    case HDMI_LAYER_GRAPHIC_0 :
        {
            if(0 < fp_tvout_g0)
            {
                close(fp_tvout_g0);
                fp_tvout_g0 = -1;
            }
            break;
        }
    case HDMI_LAYER_GRAPHIC_1 :
        {
            if(0 < fp_tvout_g1)
            {
                close(fp_tvout_g1);
                fp_tvout_g1 = -1;
            }
            break;
        }
    default :
        {

            LOGE("%s::unmathced layer(%d) fail", __func__, layer);
            ret = -1;
            break;
        }
    }

    return ret;
}

#define ROUND_UP(value, boundary) ((((uint32_t)(value)) + \
                                  (((uint32_t) boundary)-1)) & \
                                  (~(((uint32_t) boundary)-1)))

int hdmi_set_v_param(int layer,
        int src_w, int src_h, int colorFormat,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_w, int dst_h)
{
    int round_up_src_w;
    int round_up_src_h;
    if(fp_tvout_v <= 0)
    {
        LOGE("fp_tvout is < 0 fail\n");
        return -1;
    }

    /* src_w, src_h round up to DWORD because of VP restriction */
    round_up_src_w = ROUND_UP( src_w, 2*sizeof(uint32_t) );
    round_up_src_h = ROUND_UP( src_h, 2*sizeof(uint32_t) );

    vo_param.src.base_y         = (void *)src_y_address;
    vo_param.src.base_c         = (void *)src_c_address;
    vo_param.src.pix_fmt.width  = round_up_src_w;
    vo_param.src.pix_fmt.height = round_up_src_h;
    vo_param.src.pix_fmt.field  = V4L2_FIELD_NONE;
    vo_param.src.pix_fmt.pixelformat = colorFormat;
#ifdef DEBUG_HDMI_HW_LEVEL
    //LOGD("### %s(y_addr = 0x%8x, cb_addr = 0x%8x, w = %d, h= %d, pix_fmt.field = %d, pix_fmt.pixelformat = %d) called\n",
    //        __func__, vo_param.src.base_y, vo_param.src.base_c, vo_param.src.pix_fmt.width, vo_param.src.pix_fmt.height,
    //        vo_param.src.pix_fmt.field, vo_param.src.pix_fmt.pixelformat );
#endif

    tvout_v4l2_s_fmt(fp_tvout_v, V4L2_BUF_TYPE_PRIVATE, &vo_param.src);

    vo_param.src_crop.width   = src_w;
    vo_param.src_crop.height  = src_h;

    tvout_v4l2_s_crop(fp_tvout_v, V4L2_BUF_TYPE_PRIVATE, &vo_param.src_crop);

    if(dst_w * src_h <= dst_h * src_w)
    {
        vo_param.dst_win.w.left   = 0;
        vo_param.dst_win.w.top    = (dst_h - ((dst_w * src_h) / src_w)) >> 1;
        vo_param.dst_win.w.width  = dst_w;
        vo_param.dst_win.w.height = ((dst_w * src_h) / src_w);
    }
    else
    {
        vo_param.dst_win.w.left   = (dst_w - ((dst_h * src_w) / src_h)) >> 1;
        vo_param.dst_win.w.top    = 0;
        vo_param.dst_win.w.width  = ((dst_h * src_w) / src_h);
        vo_param.dst_win.w.height = dst_h;
    }

    vo_param.dst.fmt.priv = 10;
    vo_param.dst_win.global_alpha = 255;
    tvout_v4l2_s_fbuf(fp_tvout_v, &vo_param.dst);
    tvout_v4l2_s_fmt(fp_tvout_v, V4L2_BUF_TYPE_VIDEO_OVERLAY, &vo_param.dst_win);

    /*
    // sw5771.park : calibration in floating level..
    float src_ratio = (float)src_w    / (float)src_h;
    float dst_ratio = (float)dst_w / (float)dst_h;

    if(src_ratio != dst_ratio)
    {
    // narrow source :
    if(src_ratio <= dst_ratio)
    {
    tv_param.tvout_dst.width  = (dst_h * src_ratio);
    if(dst_w < tv_param.tvout_dst.width)
    tv_param.tvout_dst.width = dst_w;

    tv_param.tvout_dst.height = (dst_h);
    tv_param.tvout_dst.left   = (dst_w - tv_param.tvout_dst.width) >> 1;
    tv_param.tvout_dst.top    = 0;
    }
    // wide source
    else
    {
    tv_param.tvout_dst.width  = (dst_w);
    tv_param.tvout_dst.height = (dst_w / src_ratio);
    if(dst_h < tv_param.tvout_dst.height)
    tv_param.tvout_dst.height = dst_h;
    tv_param.tvout_dst.left   = 0;
    tv_param.tvout_dst.top    = (dst_h - tv_param.tvout_dst.height) >> 1;
    }
    }
    else
    {
    tv_param.tvout_dst.left   = 0;
    tv_param.tvout_dst.top    = 0;
    tv_param.tvout_dst.width  = dst_w;
    tv_param.tvout_dst.height = dst_h;
    }
    */
    /*
       LOGD("src_w %d \n", src_w);
       LOGD("src_h %d \n", src_h);
       LOGD("src_ratio %f \n", src_ratio);
       LOGD("dst_w %d \n", dst_w);
       LOGD("dst_h %d \n", dst_h);
       LOGD("dst_ratio %f \n", dst_ratio);

       LOGD("tv_param.tvout_dst.left  %d \n", tv_param.tvout_dst.left);
       LOGD("tv_param.tvout_dst.top    %d \n", tv_param.tvout_dst.top);
       LOGD("tv_param.tvout_dst.width  %d \n", tv_param.tvout_dst.width);
       LOGD("tv_param.tvout_dst.height %d \n", tv_param.tvout_dst.height);
       */

    return 0;
}



#if defined(BOARD_USES_HDMI_SUBTITLES)
int hdmi_gl_set_param(int layer,
        int src_w, int src_h,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_x, int dst_y, int dst_w, int dst_h)
#if defined(BOARD_USES_FIMGAPI)
{
    LOGD("gl_set_param!!");
    int          dst_color_format;
    int          dst_bpp;

    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;

    int fp_tvout_g;

    if(layer == HDMI_LAYER_GRAPHIC_0)
        fp_tvout_g = fp_tvout_g0;
    else
        fp_tvout_g = fp_tvout_g1;

    switch (t_std_id)
    {
    case V4L2_STD_1080P_60:
    case V4L2_STD_1080P_30:
    case V4L2_STD_1080I_60:
        dst_color_format = FimgApi::COLOR_FORMAT_ARGB_4444;
        dst_bpp = 2;
        var.bits_per_pixel = 16;
        var.transp.length = 4;
        break;
    case V4L2_STD_720P_60:
    case V4L2_STD_576P_50_16_9:
    case V4L2_STD_480P_60_16_9:
    default:
        dst_color_format = FimgApi::COLOR_FORMAT_ARGB_8888;
        dst_bpp = 4;
        var.bits_per_pixel = 32;
        var.transp.length = 8;
        break;
    }

    FimgRect srcRect = {0, 0,
        src_w, src_h,
        src_w, src_h,
        FimgApi::COLOR_FORMAT_ARGB_8888,
        4, (unsigned char *)src_y_address};


    FimgRect dstRect = {0, 0,
        dst_w, dst_h,
        dst_w, dst_h,
        dst_color_format,
        dst_bpp,
        NULL};

    FimgClip Clip = {dstRect.y,
        dstRect.y + dstRect.h,
        dstRect.x,
        dstRect.x + dstRect.w};


    static unsigned int flip_g2d = 0;

    if (flip_g2d & 1)
        dstRect.addr = (unsigned char *)g2d_reserved_memory1;
    else
        dstRect.addr = (unsigned char *)g2d_reserved_memory0;

    flip_g2d++;

    FimgFlag flag = {FimgApi::ROTATE_0, FimgApi::ALPHA_MAX,
        0, 0, 0, 0, 0, 0, 0, 0, 0, G2D_MEMORY_KERNEL};

    if( mUIRotVal == 90)
        flag.rotate_val = FimgApi::ROTATE_90;
    else if( mUIRotVal == 180)
        flag.rotate_val = FimgApi::ROTATE_180;
    else if( mUIRotVal == 270)
        flag.rotate_val = FimgApi::ROTATE_270;
    else
        flag.rotate_val = FimgApi::ROTATE_0;


    flag.alpha_val = G2D_ALPHA_BLENDING_OPAQUE;  // no alpha..
    flag.potterduff_mode = G2D_Clear_Mode;
    if(stretchFimgApi(&srcRect, &dstRect, &Clip, &flag) < 0)
    {
        LOGE("%s::stretchFimgApi() 1 fail \n", __func__);
        return -1;
    }

    flag.potterduff_mode = G2D_SrcOver_Mode;

    // scale and rotate and alpha with FIMG
    if(stretchFimgApi(&srcRect, &dstRect, &Clip, &flag) < 0)
    {
        LOGE("%s::stretchFimgApi() 2 fail \n", __func__);
        return -1;
    }

    if(mUIRotVal == 0 || mUIRotVal == 180)
    {
        var.xres = dst_w;
        var.yres = dst_h;
    }else{
        var.xres = dst_h;
        var.yres = dst_w;
    }

    var.xres_virtual = var.xres;
    var.yres_virtual = var.yres;
    var.xoffset = 0;
    var.yoffset = 0;
    var.width = 0;
    var.height = 0;
    var.activate = FB_ACTIVATE_FORCE;

    window.x = dst_x;
    window.y = dst_y;

    tvout_v4l2_s_baseaddr(fp_tvout_g, (void *)dstRect.addr);
    put_vscreeninfo(fp_tvout_g, &var);
    ioctl(fp_tvout_g, S5PTVFB_WIN_POSITION, &window);

    return 0;
}
#else
{
    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;

    struct overlay_param ov_param;

    // set base address for grp layer0 of mixer
    int fp_tvout_g;

    if(layer == HDMI_LAYER_GRAPHIC_0)
        fp_tvout_g = fp_tvout_g0;
    else
        fp_tvout_g = fp_tvout_g1;

    var.xres = src_w;
    var.yres = src_h;
    var.xres_virtual = var.xres;
    var.yres_virtual = var.yres;
    var.xoffset = 0;
    var.yoffset = 0;
    var.width = src_w;
    var.height = src_h;
    var.activate = FB_ACTIVATE_FORCE;
    var.bits_per_pixel = 32;
    var.transp.length = 8;

    window.x = dst_x;
    window.y = dst_y;

    tvout_v4l2_s_baseaddr(fp_tvout_g, (void *)src_y_address);
    put_vscreeninfo(fp_tvout_g, &var);
    ioctl(fp_tvout_g, S5PTVFB_WIN_POSITION, &window);

    return 0;
}
#endif
#endif

int hdmi_cable_status()
{
    int cable_status;
    int ret = 0;
    int fp_hpd;

    fp_hpd = open(HPD_DEV, O_RDWR);
    if (fp_hpd <= 0) {
        LOGE("hpd drv open failed\n");
        return -1;
    }

    ret = ioctl(fp_hpd, HPD_GET_STATE, &cable_status);
    if(ret != 0)
    {
        close(fp_hpd);
        LOGE("hpd drv HPD_GET_STATE ioctl failed\n");
        return -1;
    }
    else
    {
        close(fp_hpd);
        return cable_status;
    }
}

int hdmi_outputmode_2_v4l2_output_type(int output_mode)
{
    int v4l2_output_type = -1;

    switch(output_mode)
    {
    case HDMI_OUTPUT_MODE_YCBCR:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DIGITAL;
        break;
    case HDMI_OUTPUT_MODE_RGB:
        v4l2_output_type = V4L2_OUTPUT_TYPE_HDMI_RGB;
        break;
    case HDMI_OUTPUT_MODE_DVI:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DVI;
        break;
    case COMPOSITE_OUTPUT_MODE:
        v4l2_output_type = V4L2_OUTPUT_TYPE_COMPOSITE;
        break;
    default:
        LOGE("%s::unmathced HDMI_mode(%d)", __func__, output_mode);
        v4l2_output_type = -1;
        break;
    }

    return v4l2_output_type;
}

int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type)
{
    int outputMode = -1;

    switch(v4l2_output_type)
    {
    case V4L2_OUTPUT_TYPE_DIGITAL:
        outputMode = HDMI_OUTPUT_MODE_YCBCR;
        break;
    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        outputMode = HDMI_OUTPUT_MODE_RGB;
        break;
    case V4L2_OUTPUT_TYPE_DVI:
        outputMode = HDMI_OUTPUT_MODE_DVI;
        break;
    case V4L2_OUTPUT_TYPE_COMPOSITE:
        outputMode = COMPOSITE_OUTPUT_MODE;
        break;
    default:
        LOGE("%s::unmathced v4l2_output_type(%d)", __func__, v4l2_output_type);
        outputMode = -1;
        break;
    }

    return outputMode;
}

int composite_std_2_v4l2_std_id(int std)
{
    int std_id = -1;

    switch(std)
    {
    case COMPOSITE_STD_NTSC_M:
        std_id = V4L2_STD_NTSC_M;
        break;
    case COMPOSITE_STD_NTSC_443:
        std_id = V4L2_STD_NTSC_443;
        break;
    case COMPOSITE_STD_PAL_BDGHI:
        std_id = V4L2_STD_PAL_BDGHI;
        break;
    case COMPOSITE_STD_PAL_M:
        std_id = V4L2_STD_PAL_M;
        break;
    case COMPOSITE_STD_PAL_N:
        std_id = V4L2_STD_PAL_N;
        break;
    case COMPOSITE_STD_PAL_Nc:
        std_id = V4L2_STD_PAL_Nc;
        break;
    case COMPOSITE_STD_PAL_60:
        std_id = V4L2_STD_PAL_60;
        break;
    default:
        LOGE("%s::unmathced composite_std(%d)", __func__, std);
        break;
    }

    return std_id;
}

static int hdmi_check_output_mode(int v4l2_output_type)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;
    int    calbirate_v4l2_mode = v4l2_output_type;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    switch(v4l2_output_type)
    {
    case V4L2_OUTPUT_TYPE_DIGITAL :

        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video))
        {
            //jhkim//calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            //audio_state = NOT_SUPPORT;
            LOGI("Change mode into DVI\n");
            break;
        }
        /*else
          {
          if (EDIDAudioModeSupport(&audio))
          audio_state = ON;
          }*/

        video.colorSpace = HDMI_CS_YCBCR444;
        if (!EDIDColorSpaceSupport(&video))
        {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
            LOGI("Change mode into HDMI_RGB\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_HDMI_RGB:

        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video))
        {
            //jhkim//calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            //audio_state = NOT_SUPPORT;
            LOGI("Change mode into DVI\n");
            break;
        }
        /*else
          {
          if (EDIDAudioModeSupport(&audio))
          audio_state = ON;
          }*/
        video.colorSpace = HDMI_CS_YCBCR444;
        if (EDIDColorSpaceSupport(&video))
        {
            //jhkim//calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
            LOGI("Change mode into HDMI\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_DVI:

        video.mode = HDMI;
        if (EDIDHDMIModeSupport(&video))
        {
            video.colorSpace = HDMI_CS_YCBCR444;
            if (EDIDColorSpaceSupport(&video))
            {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
                LOGI("Change mode into HDMI_YCBCR\n");
            }
            else
            {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
                LOGI("Change mode into HDMI_RGB\n");
            }

            /*if (EDIDAudioModeSupport(&audio))
              audio_state = ON;*/
        } /*else
            audio_state = NOT_SUPPORT;*/
        break;

    default:
        break;
    }
    return calbirate_v4l2_mode;
}


static int hdmi_check_resolution(v4l2_std_id std_id)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    switch (std_id)
    {
    case V4L2_STD_480P_60_16_9:
        video.resolution = v720x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        break;
    case V4L2_STD_480P_60_4_3:
        video.resolution = v640x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        break;
    case V4L2_STD_576P_50_16_9:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        break;
    case V4L2_STD_576P_50_4_3:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        break;
    case V4L2_STD_720P_60:
        video.resolution = v1280x720p_60Hz;
        break;
    case V4L2_STD_720P_50:
        video.resolution = v1280x720p_50Hz;
        break;
    case V4L2_STD_1080P_60:
        video.resolution = v1920x1080p_60Hz;
        break;
    case V4L2_STD_1080P_50:
        video.resolution = v1920x1080p_50Hz;
        break;
    case V4L2_STD_1080I_60:
        video.resolution = v1920x1080i_60Hz;
        break;
    case V4L2_STD_1080I_50:
        video.resolution = v1920x1080i_50Hz;
        break;
    case V4L2_STD_480P_59:
        video.resolution = v720x480p_60Hz;
        break;
    case V4L2_STD_720P_59:
        video.resolution = v1280x720p_60Hz;
        break;
    case V4L2_STD_1080I_59:
        video.resolution = v1920x1080i_60Hz;
        break;
    case V4L2_STD_1080P_59:
        video.resolution = v1920x1080p_60Hz;
        break;
    case V4L2_STD_1080P_30:
        video.resolution = v1920x1080p_30Hz;
        break;
    default:
        LOGE("%s::unmathced std_id(%lld)", __func__, std_id);
        return -1;
        break;
    }

    if (!EDIDVideoResolutionSupport(&video))
    {
        LOGD("%s::EDIDVideoResolutionSupport(%llx) fail (not suppoted std_id) \n", __func__, std_id);

        return -1;
    }

    return 0;
}

int hdmi_resolution_2_std_id(unsigned int resolution, int * w, int * h, v4l2_std_id * std_id)
{
    int ret = 0;

    switch (resolution)
    {
    case 1080960:
        *std_id = V4L2_STD_1080P_60;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080950:
        *std_id = V4L2_STD_1080P_50;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080930:
        *std_id = V4L2_STD_1080P_30;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080160:
        *std_id = V4L2_STD_1080I_60;
        *w      = 1920;
        *h      = 1080;
        break;
    case 1080150:
        *std_id = V4L2_STD_1080I_50;
        *w      = 1920;
        *h      = 1080;
        break;
    case 720960:
        *std_id = V4L2_STD_720P_60;
        *w      = 1280;
        *h      = 720;
        break;
    case 720950:
        *std_id = V4L2_STD_720P_50;
        *w      = 1280;
        *h      = 720;
        break;
    case 5769501:
        *std_id = V4L2_STD_576P_50_16_9;
        *w      = 720;
        *h      = 576;
        break;
    case 5769502:
        *std_id = V4L2_STD_576P_50_4_3;
        *w      = 720;
        *h      = 576;
        break;
    case 4809601:
        *std_id = V4L2_STD_480P_60_16_9;
        *w      = 720;
        *h      = 480;
        break;
    case 4809602:
        *std_id = V4L2_STD_480P_60_4_3;
        *w     = 720;
        *h     = 480;
        break;
    default:
        LOGE("%s::unmathced resolution(%d)", __func__, resolution);
        ret = -1;
        break;
    }

    return ret;
}


//=============================
//
int hdmi_enable_hdcp(unsigned int hdcp_en)
{
    if(ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, hdcp_en) < 0)
    {
        LOGD("%s::VIDIOC_HDCP_ENABLE(%d) fail \n", __func__, hdcp_en);
        return -1;
    }

    return 0;
}

static int hdmi_check_audio(void)
{
    struct HDMIAudioParameter audio;
    enum state audio_state = ON;
    int ret = 0;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;


#if defined(BOARD_USES_EDID)
    if (!EDIDAudioModeSupport(&audio))
        audio_state = NOT_SUPPORT;
    else
        audio_state = ON;
#endif

#if 1 // force
	audio_state = ON;
    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;
#endif

    if (audio_state == ON)
        ioctl(fp_tvout, VIDIOC_INIT_AUDIO, 1);
    else
        ioctl(fp_tvout, VIDIOC_INIT_AUDIO, 0);

    return ret;
}

#if defined(BOARD_USES_CEC)
SecHdmi::CECThread::~CECThread()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s", __func__);
#endif
    mFlagRunning = false;
}

bool SecHdmi::CECThread::threadLoop()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    //LOGD("%s", __func__);
#endif
    unsigned char buffer[CEC_MAX_FRAME_SIZE];
    int size;
    unsigned char lsrc, ldst, opcode;

    {
        Mutex::Autolock lock(mThreadLoopLock);
        mFlagRunning = true;

        size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 100000);

        if (!size) { // no data available or ctrl-c
            //LOGE("CECReceiveMessage() failed!\n");
            return true;
        }

        if (size == 1)
            return true; // "Polling Message"

        lsrc = buffer[0] >> 4;

        /* ignore messages with src address == mLaddr*/
        if (lsrc == mLaddr)
            return true;

        opcode = buffer[1];

        if (CECIgnoreMessage(opcode, lsrc)) {
            LOGE("### ignore message coming from address 15 (unregistered)\n");
            return true;
        }

        if (!CECCheckMessageSize(opcode, size)) {
            LOGE("### invalid message size: %d(opcode: 0x%x) ###\n", size, opcode);
            return true;
        }

        /* check if message broadcasted/directly addressed */
        if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
            LOGE("### invalid message mode (directly addressed/broadcast) ###\n");
            return true;
        }

        ldst = lsrc;

        //TODO: macroses to extract src and dst logical addresses
        //TODO: macros to extract opcode

        switch (opcode)
        {
        case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
            /* responce with "Report Physical Address" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            buffer[4] = mDevtype;
            size = 5;
            break;

        case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
            LOGD("[CEC_OPCODE_REQUEST_ACTIVE_SOURCE]\n");
            /* responce with "Active Source" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            size = 4;
            LOGD("Tx : [CEC_OPCODE_ACTIVE_SOURCE]\n");
            break;

        case CEC_OPCODE_ABORT:
        case CEC_OPCODE_FEATURE_ABORT:
        default:
            /* send "Feature Abort" */
            buffer[0] = (mLaddr << 4) | ldst;
            buffer[1] = CEC_OPCODE_FEATURE_ABORT;
            buffer[2] = CEC_OPCODE_ABORT;
            buffer[3] = 0x04; // "refused"
            size = 4;
            break;
        }

        if (CECSendMessage(buffer, size) != size)
            LOGE("CECSendMessage() failed!!!\n");

    }
    return true;
}

bool SecHdmi::CECThread::start()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mThreadControlLock);
    if(exitPending()){
        if(requestExitAndWait() == WOULD_BLOCK)
        {
            LOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
            return false;
        }
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("EDIDGetCECPhysicalAddress");
#endif
    /* set to not valid physical address */
    mPaddr = CEC_NOT_VALID_PHYSICAL_ADDRESS;

    if (!EDIDGetCECPhysicalAddress(&mPaddr)) {
        LOGE("Error: EDIDGetCECPhysicalAddress() failed.\n");
        return false;
    }

    //LOGD("Device physical address is %X.%X.%X.%X\n",
    //       (mPaddr & 0xF000) >> 12, (mPaddr & 0x0F00) >> 8,
    //       (mPaddr & 0x00F0) >> 4, mPaddr & 0x000F);

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("CECOpen");
#endif
    if (!CECOpen()) {
        LOGE("CECOpen() failed!!!\n");
        return false;
    }

    /* a logical address should only be allocated when a device \
       has a valid physical address, at all other times a device \
       should take the 'Unregistered' logical address (15)
       */

    /* if physical address is not valid device should take \
       the 'Unregistered' logical address (15)
       */

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("CECAllocLogicalAddress");
#endif
    mLaddr = CECAllocLogicalAddress(mPaddr, mDevtype);

    if (!mLaddr) {
        LOGE("CECAllocLogicalAddress() failed!!!\n");
        if (!CECClose()) {
            LOGE("CECClose() failed!\n");
        }
        return false;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("request to run CECThread");
#endif
    //buffer = (unsigned char *)malloc(CEC_MAX_FRAME_SIZE);
    status_t ret = run("SecHdmi::CECThread", PRIORITY_DISPLAY);
    if(ret != NO_ERROR)
    {
        LOGE("%s fail to run thread", __func__);
        return false;
    }
    return true;
}

bool SecHdmi::CECThread::stop()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s request Exit", __func__);
#endif
    Mutex::Autolock lock(mThreadControlLock);
    if(requestExitAndWait() == WOULD_BLOCK)
    {
        LOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
        return false;
    }

    if (!CECClose()) {
        LOGE("CECClose() failed!\n");
    }

    mFlagRunning = false;
    return true;
}
#endif

////////////////////////////////////////////////////////////////////////
SecHdmi::SecHdmi():
#if defined(BOARD_USES_CEC)
    mCECThread(NULL),
#endif
    mFlagCreate(false),
    mFlagConnected(false),
    //mFlagHdmiStart(false),
    mHdmiDstWidth(0),
    mHdmiDstHeight(0),
    mHdmiSrcYAddr(0),
    mHdmiSrcCbCrAddr(0),
    mHdmiOutputMode(DEFAULT_OUPUT_MODE),
    mHdmiResolutionValue(DEFAULT_HDMI_RESOLUTION_VALUE), // V4L2_STD_480P_60_4_3
    mHdmiStdId(DEFAULT_HDMI_STD_ID), // V4L2_STD_480P_60_4_3
    mCompositeStd(DEFAULT_COMPOSITE_STD),
    mHdcpMode(false),
    mAudioMode(2),
    mUIRotVal(0),
    mCurrentHdmiOutputMode(-1),
    mCurrentHdmiResolutionValue(0), // 1080960
    mCurrentHdcpMode(false),
    mCurrentAudioMode(-1),
    mHdmiInfoChange(true),
    mFlagFimcStart(false),
    mFimcDstColorFormat(0),
    mFimcCurrentOutBufIndex(0),
    mDefaultFBFd(-1)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s", __func__);
#endif
    for(int i = 0; i < HDMI_LAYER_MAX; i++)
    {
        mFlagLayerEnable[i] = false;
        mFlagHdmiStart[i] = false;

        mSrcWidth      [i] = 0;
        mSrcHeight     [i] = 0;
        mSrcColorFormat[i] = 0;
        mHdmiSrcWidth  [i] = 0;
        mHdmiSrcHeight [i] = 0;
    }

    // Video layer is always on
    mFlagLayerEnable[HDMI_LAYER_VIDEO] = true;

    mHdmiSizeOfResolutionValueList = 11;

    mHdmiResolutionValueList[0]  = 1080960;
    mHdmiResolutionValueList[1]  = 1080950;
    mHdmiResolutionValueList[2]  = 1080930;
    mHdmiResolutionValueList[3]  = 1080160;
    mHdmiResolutionValueList[4]  = 1080150;
    mHdmiResolutionValueList[5]  = 720960;
    mHdmiResolutionValueList[6]  = 720950;
    mHdmiResolutionValueList[7]  = 5769501;
    mHdmiResolutionValueList[8]  = 5769502;
    mHdmiResolutionValueList[9]  = 4809601;
    mHdmiResolutionValueList[10] = 4809602;

#if defined(BOARD_USES_CEC)
    mCECThread = new CECThread(this);
#endif

    for(int i=0; i<HDMI_FIMC_OUTPUT_BUF_NUM; i++)
    {
        mFimcReservedMem[i]= 0;
    }
}

SecHdmi::~SecHdmi()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s", __func__);
#endif
    if(mFlagCreate == true)
        LOGE("%s::this is not Destroyed fail \n", __func__);
    else
    {
        disconnect();
    }
}

bool SecHdmi::create(void)
{
    Mutex::Autolock lock(mLock);
    unsigned int fimcReservedMemStart;
    unsigned int fimc_buf_size = 2<<20;
    mFimcCurrentOutBufIndex = 0;

    if(mFlagCreate == true)
    {
        LOGE("%s::Already Created fail \n", __func__);
        goto CREATE_FAIL;
    }

    if(mDefaultFBFd <= 0)
    {
        if((mDefaultFBFd = fb_open(DEFAULT_FB)) < 0)
        {
            LOGE("%s:Failed to open default FB\n", __func__);
            return false;
        }
    }

    if(mSecFimc.create(SecFimc::FIMC_DEV3, FIMC_OVLY_NONE_SINGLE_BUF, 1) == false)
    {
        LOGE("%s::SecFimc create fail \n", __func__);
        goto CREATE_FAIL;
    }

    fimcReservedMemStart = mSecFimc.getFimcRsrvedPhysMemAddr();

    for(int i=0; i < HDMI_FIMC_OUTPUT_BUF_NUM; i++)
    {
        mFimcReservedMem[i] = fimcReservedMemStart + (fimc_buf_size * i);
    }


    v4l2_std_id std_id;
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s::mHdmiOutputMode(%d) \n", __func__, mHdmiOutputMode);
#endif
    if(mHdmiOutputMode == COMPOSITE_OUTPUT_MODE)
    {
        std_id = composite_std_2_v4l2_std_id(mCompositeStd);
        if((int)std_id < 0)
        {
            LOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
        if(m_setCompositeResolution(mCompositeStd) == false)
        {
            LOGE("%s::m_setCompositeResolution(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
    }
    else if(mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
    {
        if(hdmi_resolution_2_std_id(mHdmiResolutionValue, &mHdmiDstWidth, &mHdmiDstHeight, &std_id) < 0)
        {
            LOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, mHdmiResolutionValue);
            goto CREATE_FAIL;
        }
    }

#if defined(BOARD_USES_FIMGAPI)
    {
        if(g2d_reserved_memory0 == 0)
        {
            int fd = open(SEC_G2D_DEV_NAME, O_RDWR);
            if(fd < 0)
            {
                LOGE("%s:: failed to open G2D", __func__);
                goto CREATE_FAIL;
            }

            if(ioctl(fd, G2D_GET_MEMORY, &g2d_reserved_memory0) < 0)
            {
                LOGE("%s::S3C_G2D_GET_MEMORY fail\n", __func__);
                close(fd);
                goto CREATE_FAIL;
            }

            if(ioctl(fd, G2D_GET_MEMORY_SIZE, &g2d_reserved_memory_size) < 0)
            {
                LOGE("%s::S3C_G2D_GET_MEMORY_SIZE fail\n", __func__);
                close(fd);
                goto CREATE_FAIL;
            }

            close(fd);
        }

        /* g2d reserved memory is allocated 4M for double buffering.
         * g2d_reserved_memory0 is for front buffer.
         * g2d_reserved_meonry1 is for back buffer.
         */
        if(g2d_reserved_memory1 == 0)
            g2d_reserved_memory1 = g2d_reserved_memory0 + (g2d_reserved_memory_size >> 1);
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s: G2D Reserved memory (0x%8x, 0x%8x)", __func__, g2d_reserved_memory0, g2d_reserved_memory1);
#endif
    }
#endif

    mFlagCreate = true;

    return true;

CREATE_FAIL :

    if(mSecFimc.flagCreate() == true &&
       mSecFimc.destroy()    == false)
    {
        LOGE("%s::fimc destory fail \n", __func__);
    }

    return false;
}

bool SecHdmi::destroy(void)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Already Destroyed fail \n", __func__);
        goto DESTROY_FAIL;
    }

#if defined(BOARD_USES_HDMI_SUBTITLES)
    for(int layer = HDMI_LAYER_BASE + 1; layer <= HDMI_LAYER_GRAPHIC_0; layer++)
    {
        if(mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false)
        {
            LOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }

        if(hdmi_deinit_layer(layer) < 0)
        {
            LOGE("%s::hdmi_deinit_layer(%d) fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }
    }
#else
    if(mFlagHdmiStart[HDMI_LAYER_VIDEO] == true && m_stopHdmi(HDMI_LAYER_VIDEO) == false)
    {
        LOGE("%s::m_stopHdmi: layer fail \n", __func__);
        goto DESTROY_FAIL;
    }

    if(hdmi_deinit_layer(HDMI_LAYER_VIDEO) < 0)
    {
        LOGE("%s::hdmi_deinit_layer fail \n", __func__);
        goto DESTROY_FAIL;
    }
#endif
    tvout_deinit();

    if(   mSecFimc.flagCreate() == true
            && mSecFimc.destroy()    == false)
    {
        LOGE("%s::fimc destory fail \n", __func__);
        goto DESTROY_FAIL;
    }

#ifdef USE_LCD_ADDR_IN_HERE
    {
        if(0 < mDefaultFBFd)
        {
            close(mDefaultFBFd);
            mDefaultFBFd = -1;
        }
    }
#endif //USE_LCD_ADDR_IN_HERE

    mFlagCreate = false;

    return true;

DESTROY_FAIL :

    return false;
}

bool SecHdmi::connect(void)
{
    {
        Mutex::Autolock lock(mLock);

        if(mFlagCreate == false)
        {
            LOGE("%s::Not Yet Created \n", __func__);
            return false;
        }

        if(mFlagConnected == true)
        {
            LOGD("%s::Already Connected.. \n", __func__);
            return true;
        }

        if(mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
        {
            if(m_flagHWConnected() == false)
            {
                LOGD("%s::m_flagHWConnected() fail \n", __func__);
                return false;
            }

#if defined(BOARD_USES_EDID)
            if (!EDIDOpen())
            {
                LOGE("EDIDInit() failed!\n");
            }

            if (!EDIDRead())
            {
                LOGE("EDIDRead() failed!\n");
                if (!EDIDClose())
                    LOGE("EDIDClose() failed!\n");
            }
#endif

#if defined(BOARD_USES_CEC)
            if(!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif
        }
    }

    if(this->setHdmiOutputMode(mHdmiOutputMode, true) == false)
        LOGE("%s::setHdmiOutputMode(%d) fail \n", __func__, mHdmiOutputMode);

    if(mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
    {
        if(this->setHdmiResolution(mHdmiResolutionValue, true) == false)
            LOGE("%s::setHdmiResolution(%d) fail \n", __func__, mHdmiResolutionValue);

        if(this->setHdcpMode(mHdcpMode, false) == false)
            LOGE("%s::setHdcpMode(%d) fail \n", __func__, mHdcpMode);

        if(this->m_setAudioMode(mAudioMode) == false)
            LOGE("%s::m_setAudioMode(%d) fail \n", __func__, mAudioMode);

#if defined(BOARD_USES_EDID)
        // show display..
        display_menu();
#endif
    }

    mHdmiInfoChange = true;

    mFlagConnected = true;


    return true;
}

bool SecHdmi::disconnect(void)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(mFlagConnected == false)
    {
        LOGE("%s::Already Disconnected.. \n", __func__);
        return true;
    }

    if( mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
    {
#if defined(BOARD_USES_CEC)
        if(mCECThread->mFlagRunning)
            mCECThread->stop();
#endif

#if defined(BOARD_USES_EDID)
        if (!EDIDClose())
        {
            LOGE("EDIDClose() failed!\n");
            return false;
        }
#endif
    }

    for(int layer = SecHdmi::HDMI_LAYER_BASE + 1; layer <= SecHdmi::HDMI_LAYER_GRAPHIC_0; layer++)
    {
        if(mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false)
        {
            LOGE("%s::hdmiLayer(%d) layer fail \n", __func__, layer);
            return false;
        }
    }
    tvout_deinit();

    mFlagConnected = false;

    mHdmiOutputMode = DEFAULT_OUPUT_MODE;
    mHdmiResolutionValue = DEFAULT_HDMI_RESOLUTION_VALUE;
    mHdmiStdId = DEFAULT_HDMI_STD_ID;
    mCompositeStd = DEFAULT_COMPOSITE_STD;
    mAudioMode = 2;
    mCurrentHdmiOutputMode = -1;
    mCurrentHdmiResolutionValue = 0;
    mCurrentAudioMode = -1;
    mFimcCurrentOutBufIndex = 0;

    return true;
}

bool SecHdmi::startHdmi(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);
    if(mFlagHdmiStart[hdmiLayer] == false &&
            m_startHdmi(hdmiLayer) == false)
    {
        LOGE("%s::hdmiLayer(%d) fail \n", __func__, hdmiLayer);
        return false;
    }
    return true;
}

bool SecHdmi::stopHdmi(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);
    if(mFlagHdmiStart[hdmiLayer] == true &&
            m_stopHdmi(hdmiLayer) == false)
    {
        LOGE("%s::hdmiLayer(%d) layer fail \n", __func__, hdmiLayer);
        return false;
    }
    tvout_deinit();
    return true;
}

bool SecHdmi::flagConnected(void)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    return mFlagConnected;
}


bool SecHdmi::flush(int srcW, int srcH, int srcColorFormat,
        unsigned int srcPhysYAddr, unsigned int srcPhysCbAddr,
        int dstX, int dstY,
        int hdmiLayer)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(srcPhysYAddr == 0)
    {
        unsigned int phyFBAddr = 0;

        // get physical framebuffer address for LCD
        if (ioctl(mDefaultFBFd, S3CFB_GET_FB_PHY_ADDR, &phyFBAddr) == -1)
        {
            LOGE("%s:ioctl(S3CFB_GET_FB_PHY__ADDR) fail\n", __func__);
            return false;
        }

        // when early suspend, FIMD IP off.
        // so physical framebuffer address for LCD is 0x00000000
        // so JUST RETURN.
        if(phyFBAddr == 0)
        {
            LOGE("%s::S3CFB_GET_FB_PHY_ADDR fail \n", __func__);
            return true;
        }

        srcPhysYAddr = phyFBAddr;
        srcPhysCbAddr = srcPhysYAddr;
    }

    /*
       if(srcPhysYAddr == 0)
       {
       LOGE("%s::srcPhysYAddr == 0 \n", __func__);
       return false;
       }
       */

    if( srcW               != mSrcWidth      [hdmiLayer] ||
        srcH               != mSrcHeight     [hdmiLayer] ||
        srcColorFormat     != mSrcColorFormat[hdmiLayer] ||
        mHdmiInfoChange == true)
    {
        if(m_reset(srcW, srcH, srcColorFormat, hdmiLayer) < 0)
        {
            LOGE("%s::m_reset(%d, %d, %d, %d) fail \n", __func__, srcW, srcH, srcColorFormat, hdmiLayer);
            return false;
        }
    }

    if(hdmiLayer == HDMI_LAYER_VIDEO)
    {
        if(srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP)
        {
            hdmi_set_v_param(hdmiLayer,
                    srcW, srcH, V4L2_PIX_FMT_NV12T,
                    srcPhysYAddr, srcPhysCbAddr,
                    mHdmiDstWidth, mHdmiDstHeight);
        }
        else if(srcColorFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP )
        {
            hdmi_set_v_param(hdmiLayer,
                    srcW, srcH, V4L2_PIX_FMT_NV21,
                    srcPhysYAddr, srcPhysCbAddr,
                    mHdmiDstWidth, mHdmiDstHeight);
        }
        else
        {
            if(mSecFimc.setSrcPhyAddr(srcPhysYAddr, srcPhysCbAddr) < 0)
            {
                LOGE("%s::mSecFimc.setSrcPhyAddr(%d, %d) fail \n",
                        __func__, srcPhysYAddr, srcPhysCbAddr);
                return false;
            }

            int  y_size = 0;
            if(mUIRotVal == 0 || mUIRotVal == 180)
            {
                y_size =  ALIGN(ALIGN(srcW,128) * ALIGN(srcH, 32), SZ_8K);
            }
            else
            {
                y_size =  ALIGN(ALIGN(srcH,128) * ALIGN(srcW, 32), SZ_8K);
            }

            if( mSecFimc.setDstPhyAddr(mFimcReservedMem[mFimcCurrentOutBufIndex],
                        mFimcReservedMem[mFimcCurrentOutBufIndex] + y_size) < 0)
            {
                LOGE("%s::mSecFimc.setDstPhyAddr(%d, %d) fail \n",
                        __func__, mFimcReservedMem[mFimcCurrentOutBufIndex],
                        mFimcReservedMem[mFimcCurrentOutBufIndex] + y_size);
                return false;
            }

            if(mSecFimc.handleOneShot() < 0)
            {
                LOGE("%s::mSecFimc.handleOneshot() fail \n", __func__);
                return false;
            }

            mHdmiSrcYAddr    = mFimcReservedMem[mFimcCurrentOutBufIndex];
            mHdmiSrcCbCrAddr = mFimcReservedMem[mFimcCurrentOutBufIndex] + y_size;

            if(mUIRotVal == 0 || mUIRotVal == 180)
            {
                hdmi_set_v_param(hdmiLayer,
                        srcW, srcH, V4L2_PIX_FMT_NV12T,
                        mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                        mHdmiDstWidth, mHdmiDstHeight);
            }
            else
            {
                hdmi_set_v_param(hdmiLayer,
                        srcH, srcW, V4L2_PIX_FMT_NV12T,
                        mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                        mHdmiDstWidth, mHdmiDstHeight);
            }

            mFimcCurrentOutBufIndex++;
            if(mFimcCurrentOutBufIndex >= HDMI_FIMC_OUTPUT_BUF_NUM)
            {
                mFimcCurrentOutBufIndex = 0;
            }
        }

    }
    else // if(hdmiLayer == HDMI_LAYER_GRAPHIC_0)
    {

#if defined(BOARD_USES_HDMI_SUBTITLES)
        if(mFlagLayerEnable[HDMI_LAYER_GRAPHIC_0])
            hdmi_gl_set_param(hdmiLayer,
                    srcW, srcH,
                    srcPhysYAddr, srcPhysCbAddr,
                    dstX, dstY, mHdmiDstWidth, mHdmiDstHeight);
#endif

    }

    if(mFlagConnected)
    {
        if(mFlagHdmiStart[hdmiLayer] == false && m_startHdmi(hdmiLayer) == false)
        {
            LOGE("%s::hdmiLayer(%d) fail \n", __func__, hdmiLayer);
            return false;
        }
    }

    return true;
}


bool SecHdmi::clear(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(mFlagHdmiStart[hdmiLayer] == true && m_stopHdmi(hdmiLayer) == false)
    {
        LOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, hdmiLayer);
        return false;
    }

    return true;
}

bool SecHdmi::enableGraphicLayer(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s::hdmiLayer(%d)",__func__, hdmiLayer);
#endif
    switch(hdmiLayer)
    {
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        mFlagLayerEnable[hdmiLayer] = true;
        if(mFlagConnected == true)
            m_startHdmi(hdmiLayer);
        break;
    default:
        return false;
    }
    return true;
}

bool SecHdmi::disableGraphicLayer(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("%s::hdmiLayer(%d)",__func__, hdmiLayer);
#endif
    switch(hdmiLayer)
    {
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        if(mFlagConnected == true && mFlagLayerEnable[hdmiLayer])
            m_stopHdmi(hdmiLayer);
        mFlagLayerEnable[hdmiLayer] = false;
        break;
    default:
        return false;
    }
    return true;
}

bool SecHdmi::setHdmiOutputMode(int hdmiOutputMode, bool forceRun)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(forceRun == false && mHdmiOutputMode == hdmiOutputMode)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
#endif
        return true;
    }

    int newHdmiOutputMode = hdmiOutputMode;

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if(v4l2OutputType < 0)
    {
        LOGD("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

#if defined(BOARD_USES_EDID)
    int newV4l2OutputType = hdmi_check_output_mode(v4l2OutputType);
    if(newV4l2OutputType != v4l2OutputType)
    {
        newHdmiOutputMode = hdmi_v4l2_output_type_2_outputmode(newV4l2OutputType);
        if(newHdmiOutputMode < 0)
        {
            LOGD("%s::hdmi_v4l2_output_type_2_outputmode(%d) fail\n", __func__, newV4l2OutputType);
            return false;
        }

        LOGD("%s::calibration mode(%d -> %d)... \n", __func__, hdmiOutputMode, newHdmiOutputMode);
        mHdmiInfoChange = true;
    }
#endif

    if(mHdmiOutputMode != newHdmiOutputMode)
    {
        mHdmiOutputMode = newHdmiOutputMode;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdmiResolution(unsigned int hdmiResolutionValue, bool forceRun)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(forceRun == false && mHdmiResolutionValue == hdmiResolutionValue)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same hdmiResolutionValue(%d) \n", __func__, hdmiResolutionValue);
#endif
        return true;
    }

    unsigned int newHdmiResolutionValue = hdmiResolutionValue;
    int w = 0;
    int h = 0;
    v4l2_std_id std_id;
    mFimcCurrentOutBufIndex = 0;

#if defined(BOARD_USES_EDID)
    // find perfect resolutions..
    if( hdmi_resolution_2_std_id(newHdmiResolutionValue, &w, &h, &std_id) < 0 ||
        hdmi_check_resolution(std_id) < 0)
    {
        bool flagFoundIndex = false;
        int resolutionValueIndex = m_resolutionValueIndex(newHdmiResolutionValue);

        for(int i = resolutionValueIndex + 1; i < mHdmiSizeOfResolutionValueList; i++)
        {
            if( hdmi_resolution_2_std_id(mHdmiResolutionValueList[i], &w, &h, &std_id) == 0 &&
                hdmi_check_resolution(std_id) == 0)
            {
                newHdmiResolutionValue = mHdmiResolutionValueList[i];
                flagFoundIndex = true;
                break;
            }
        }

        if(flagFoundIndex == false)
        {
            LOGE("%s::hdmi cannot control this resolution(%d) fail \n", __func__, hdmiResolutionValue);
            // Set resolution to 480P
            newHdmiResolutionValue = mHdmiResolutionValueList[mHdmiSizeOfResolutionValueList-2];
        }
        else
            LOGD("%s::HDMI resolutions size is calibrated(%d -> %d)..\n", __func__, hdmiResolutionValue, newHdmiResolutionValue);
    }
    else
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::find resolutions(%d) at once\n", __func__, hdmiResolutionValue);
#endif
    }
#endif

    if(mHdmiResolutionValue != newHdmiResolutionValue)
    {
        mHdmiResolutionValue = newHdmiResolutionValue;
        mHdmiInfoChange = true;
    }

    return true;
}


bool SecHdmi::setHdcpMode(bool hdcpMode, bool forceRun)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(forceRun == false && mHdcpMode == hdcpMode)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
#endif
        return true;
    }

    mHdcpMode = hdcpMode;
    mHdmiInfoChange = true;

    return true;
}

bool SecHdmi::setUIRotation(unsigned int rotVal)
{
    Mutex::Autolock lock(mLock);

    if(mFlagCreate == false)
    {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if(rotVal % 90 != 0)
    {
        LOGE("%s::Invalid rotation value(%d)", __func__, rotVal);
        return false;
    }
    if(rotVal != mUIRotVal)
    {
        mSecFimc.setRotVal(rotVal);
        mUIRotVal = rotVal;
        mHdmiInfoChange = true;
    }
    return true;
}

bool SecHdmi::m_reset(int w, int h, int colorFormat, int hdmiLayer)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif
    v4l2_std_id std_id = 0;
    mFimcCurrentOutBufIndex = 0;


    if( w               != mSrcWidth      [hdmiLayer] ||
        h               != mSrcHeight     [hdmiLayer] ||
        colorFormat     != mSrcColorFormat[hdmiLayer])
    {
        int preVideoSrcColorFormat = mSrcColorFormat[hdmiLayer];
        int videoSrcColorFormat = colorFormat;
        if(preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
           preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
           preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP)
        {
            preVideoSrcColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP;
        }

        if(hdmiLayer == HDMI_LAYER_VIDEO)
        {
            if(colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
               colorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
               colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP)
            {
#ifdef DEBUG_HDMI_HW_LEVEL
                LOGD("### %s  call mSecFimc.setSrcParams\n", __func__);
#endif
                if(mSecFimc.setSrcParams((unsigned int)w, (unsigned int)h, 0, 0,
                            (unsigned int*)&w, (unsigned int*)&h, colorFormat, true) < 0)
                {
                    LOGE("%s::mSecFimc.setSrcParams(%d, %d, %d) fail \n",
                            __func__, w, h, colorFormat);
                    return false;
                }

                mFimcDstColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP;

#ifdef DEBUG_HDMI_HW_LEVEL
                LOGD("### %s  call mSecFimc.setDstParams\n", __func__);
#endif
                if(mUIRotVal == 0 || mUIRotVal == 180)
                {
                    if(mSecFimc.setDstParams((unsigned int)w, (unsigned int)h, 0, 0,
                                (unsigned int*)&w, (unsigned int*)&h, mFimcDstColorFormat, true) < 0)
                    {
                        LOGE("%s::mSecFimc.setDstParams(%d, %d, %d) fail \n",
                                __func__, w, h, mFimcDstColorFormat);
                        return false;
                    }
                }else
                {
                    if(mSecFimc.setDstParams((unsigned int)h, (unsigned int)w, 0, 0,
                                (unsigned int*)&h, (unsigned int*)&w, mFimcDstColorFormat, true) < 0)
                    {
                        LOGE("%s::mSecFimc.setDstParams(%d, %d, %d) fail \n",
                                __func__, w, h, mFimcDstColorFormat);
                        return false;
                    }
                }
                videoSrcColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP;
            }
        }
        if(preVideoSrcColorFormat != videoSrcColorFormat)
            mHdmiInfoChange = true;

        mSrcWidth      [hdmiLayer] = w;
        mSrcHeight     [hdmiLayer] = h;
        mSrcColorFormat[hdmiLayer] = colorFormat;

        mHdmiSrcWidth  [hdmiLayer] = w;
        mHdmiSrcHeight [hdmiLayer] = h;
    }

    if( mHdmiInfoChange == true )
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("mHdmiInfoChange: %d\n", mHdmiInfoChange);
#endif
        // stop all..
#if defined(BOARD_USES_CEC)
        if( mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
        {
            if(mCECThread->mFlagRunning)
                mCECThread->stop();
        }
#endif

#if defined(BOARD_USES_HDMI_SUBTITLES)
        for(int layer = HDMI_LAYER_BASE + 1; layer <= HDMI_LAYER_GRAPHIC_0; layer++)
        {
            if(mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false)
            {
                LOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, layer);
                return false;
            }
        }
#else
        if( mFlagHdmiStart[HDMI_LAYER_VIDEO] == true &&
            m_stopHdmi(HDMI_LAYER_VIDEO) == false)
        {
            LOGE("%s::m_stopHdmi: HDMI_LAYER_VIDEO layer fail \n", __func__);
            return false;
        }
#endif

#if 0
        for(int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++)
        {
            if(hdmi_deinit_layer(layer) < 0)
                LOGE("%s::hdmi_deinit_layer(%d) fail \n", __func__, layer);
        }
#endif
        //hdmi_deinit_layer(HDMI_LAYER_VIDEO);
        //tvout_deinit();

        if(m_setHdmiOutputMode(mHdmiOutputMode) == false)
        {
            LOGE("%s::m_setHdmiOutputMode() fail \n", __func__);
            return false;
        }
        if(mHdmiOutputMode == COMPOSITE_OUTPUT_MODE)
        {
            std_id = composite_std_2_v4l2_std_id(mCompositeStd);
            if((int)std_id < 0)
            {
                LOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
                return false;
            }
            if(m_setCompositeResolution(mCompositeStd) == false)
            {
                LOGE("%s::m_setCompositeRsolution() fail \n", __func__);
                return false;
            }
        }
        else if(mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
        {
            if(m_setHdmiResolution(mHdmiResolutionValue) == false)
            {
                LOGE("%s::m_setHdmiResolution() fail \n", __func__);
                return false;
            }

            if(m_setHdcpMode(mHdcpMode) == false)
            {
                LOGE("%s::m_setHdcpMode() fail \n", __func__);
                return false;
            }
            std_id = mHdmiStdId;
        }

        fp_tvout = tvout_init(std_id);
#if defined(BOARD_USES_HDMI_SUBTITLES)
        for(int layer = HDMI_LAYER_BASE + 1; layer <= HDMI_LAYER_GRAPHIC_0; layer++)
        {
            if(hdmi_init_layer(layer) < 0)
            {
                LOGE("%s::hdmi_init_layer(%d) fail \n", __func__, layer);
            }
        }
#else
        hdmi_init_layer(HDMI_LAYER_VIDEO);
#endif

        if( mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
        {
#if defined(BOARD_USES_CEC)
            if(!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif

            if(m_setAudioMode(mAudioMode) == false)
            {
                LOGE("%s::m_setAudioMode() fail \n", __func__);
            }
        }

        mHdmiInfoChange = false;
    }

    return true;
}

bool SecHdmi::m_startHdmi(int hdmiLayer)
{
    bool ret = true;
    if(mFlagHdmiStart[hdmiLayer] == true)
    {
        LOGD("%s::already HDMI(%d layer) started.. \n", __func__, hdmiLayer);
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s: hdmiLayer(%d) called\n", __func__, hdmiLayer);
#endif

    switch(hdmiLayer)
    {
    case HDMI_LAYER_VIDEO:
        tvout_v4l2_start_overlay(fp_tvout_v);
        mFlagHdmiStart[hdmiLayer] = true;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if(mFlagLayerEnable[hdmiLayer])
        {
            ioctl(fp_tvout_g0, FBIOBLANK, (void *)FB_BLANK_UNBLANK);
            mFlagHdmiStart[hdmiLayer] = true;
        }
    case HDMI_LAYER_GRAPHIC_1 :
        if(mFlagLayerEnable[hdmiLayer])
        {
            ioctl(fp_tvout_g1, FBIOBLANK, (void *)FB_BLANK_UNBLANK);
            mFlagHdmiStart[hdmiLayer] = true;
        }
        break;
    default :
    {
        LOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }
    }


    return true;
}

bool SecHdmi::m_stopHdmi(int hdmiLayer)
{
    bool ret = true;
    if(mFlagHdmiStart[hdmiLayer] == false)
    {
        LOGD("%s::already HDMI(%d layer) stopped.. \n", __func__, hdmiLayer);
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s : layer[%d] called\n", __func__, hdmiLayer);
#endif

    switch(hdmiLayer)
    {
    case HDMI_LAYER_VIDEO:
        tvout_v4l2_stop_overlay(fp_tvout_v);
        mFlagHdmiStart[hdmiLayer] = false;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if(mFlagLayerEnable[hdmiLayer])
        {
            ioctl(fp_tvout_g0, FBIOBLANK, (void *)FB_BLANK_POWERDOWN);
            mFlagHdmiStart[hdmiLayer] = false;
        }
    case HDMI_LAYER_GRAPHIC_1 :
        if(mFlagLayerEnable[hdmiLayer])
        {
            ioctl(fp_tvout_g1, FBIOBLANK, (void *)FB_BLANK_POWERDOWN);
            mFlagHdmiStart[hdmiLayer] = false;
        }
        break;
    default :
    {
        LOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }
    }

    return true;
}

bool SecHdmi::m_setHdmiOutputMode(int hdmiOutputMode)
{
    if(hdmiOutputMode == mCurrentHdmiOutputMode)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if(v4l2OutputType < 0)
    {
        LOGE("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

    output_type = v4l2OutputType;

    mCurrentHdmiOutputMode = hdmiOutputMode;

    return true;

}

bool SecHdmi::m_setCompositeResolution(unsigned int compositeStdId)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif

    int w = 0;
    int h = 0;

    if(mHdmiOutputMode != COMPOSITE_OUTPUT_MODE)
    {
        LOGE("%s:: not supported output type \n", __func__);
        return false;
    }

    switch(compositeStdId)
    {
    case COMPOSITE_STD_NTSC_M:
    case COMPOSITE_STD_NTSC_443:
        w = 704;
        h = 480;
        break;
    case COMPOSITE_STD_PAL_BDGHI:
    case COMPOSITE_STD_PAL_M:
    case COMPOSITE_STD_PAL_N:
    case COMPOSITE_STD_PAL_Nc:
    case COMPOSITE_STD_PAL_60:
        w = 704;
        h = 576;
        break;
    default:
        LOGE("%s::unmathced composite_std(%d)", __func__, compositeStdId);
        return false;
    }

    // kcoolsw : need to remove..
    t_std_id      = composite_std_2_v4l2_std_id(mCompositeStd);

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = -1;
    return true;
}

bool SecHdmi::m_setHdmiResolution(unsigned int hdmiResolutionValue)
{
    if(hdmiResolutionValue == mCurrentHdmiResolutionValue)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same hdmiResolutionValue(%d) \n", __func__, hdmiResolutionValue);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif

    int w = 0;
    int h = 0;

    v4l2_std_id std_id;
    if( mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI)
    {
        if(hdmi_resolution_2_std_id(hdmiResolutionValue, &w, &h, &std_id) < 0)
        {
            LOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, hdmiResolutionValue);
            return false;
        }
        mHdmiStdId    = std_id;
    }
    else
    {
        LOGE("%s:: not supported output type \n", __func__);
        return false;
    }

    // kcoolsw : need to remove..
    t_std_id      = std_id;

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = hdmiResolutionValue;

    return true;
}

bool SecHdmi::m_setHdcpMode(bool hdcpMode)
{
    if(hdcpMode == mCurrentHdcpMode)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
#endif

        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif

    if(hdcpMode == true)
        g_hdcp_en = 1;
    else
        g_hdcp_en = 0;

    mCurrentHdcpMode = hdcpMode;

    return true;
}

bool SecHdmi::m_setAudioMode(int audioMode)
{
    if(audioMode == mCurrentAudioMode)
    {
#ifdef DEBUG_HDMI_HW_LEVEL
        LOGD("%s::same audioMode(%d) \n", __func__, audioMode);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif

    if(hdmi_check_audio() < 0)
    {
        LOGE("%s::hdmi_check_audio() fail \n", __func__);
        return false;
    }

    mCurrentAudioMode = audioMode;

    return true;
}

int SecHdmi::m_resolutionValueIndex(unsigned int ResolutionValue)
{
    int index = -1;

    for(int i = 0; i < mHdmiSizeOfResolutionValueList; i++)
    {
        if(mHdmiResolutionValueList[i] == ResolutionValue)
        {
            index = i;
            break;
        }
    }
    return index;
}


bool SecHdmi::m_flagHWConnected(void)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    LOGD("### %s called\n", __func__);
#endif

    bool ret = true;
    int hdmiStatus = hdmi_cable_status();

    if(hdmiStatus <= 0)
    {
        LOGD("%s::hdmi_cable_status(%d) \n", __func__, hdmiStatus);

        ret = false;
    }
    else
        ret = true;

    return ret;
}

}; // namespace android
