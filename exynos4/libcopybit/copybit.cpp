/*
 * Copyright (C) 2008 The Android Open Source Project
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
 **
 ** @author siva krishna neeli(siva.neeli@samsung.com)
 ** @date   2009-02-27
 */

#define LOG_TAG "copybit"

//#define MEASURE_STRETCH_DURATION
//#define MEASURE_BLIT_DURATION

#include <cutils/log.h>

#include <linux/fb.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <pthread.h>

#include <hardware/copybit.h>
#include <linux/android_pmem.h>

#include "utils/Timers.h"
#include "s3c_mem.h"
#include "s3c_lcd.h"
#include "gralloc_priv.h"
#include "s5p_pp.h"

//------------ DEFINE ----------------------------------------------------//
#ifndef DEFAULT_FB_NUM
#error "WHY DON'T YOU SET FB NUM.."
//#define DEFAULT_FB_NUM 0
#endif

#define USE_SINGLE_INSTANCE
#define ADJUST_PMEM

#define S3C_TRANSP_NOP	0xffffffff
#define S3C_ALPHA_NOP 0xff

typedef struct _s3c_fb_t {
    unsigned int    width;
    unsigned int    height;
}s3c_fb_t;

struct copybit_context_t {
    struct copybit_device_t device;
    s3c_fb_t        s3c_fb;
    void            *s5p_pp;
    uint8_t         mAlpha;
    uint8_t         mFlags;

#ifdef USE_SINGLE_INSTANCE
    // the number of instances to open copybit module
    unsigned int    count;
#endif
};

//----------------------- Common hardware methods ------------------------//

static int open_copybit (const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);
static int close_copybit(struct hw_device_t *dev);

static int set_parameter_copybit(struct copybit_device_t *dev, int name,
        int value) ;
static int get_parameter_copybit(struct copybit_device_t *dev, int name);
static int stretch_copybit(struct copybit_device_t *dev,
        struct copybit_image_t  const *dst,
        struct copybit_image_t  const *src,
        struct copybit_rect_t   const *dst_rect,
        struct copybit_rect_t   const *src_rect,
        struct copybit_region_t const *region);
static int blit_copybit(struct copybit_device_t *dev,
        struct copybit_image_t  const *dst,
        struct copybit_image_t  const *src,
        struct copybit_region_t const *region);

//---------------------- The COPYBIT Module -----------------------------//

static struct hw_module_methods_t copybit_module_methods = {
open: open_copybit
};

struct copybit_module_t HAL_MODULE_INFO_SYM = {
common: {
tag: HARDWARE_MODULE_TAG,
     version_major: 1,
     version_minor: 0,
     id: COPYBIT_HARDWARE_MODULE_ID,
     name: "Samsung S5P C210 COPYBIT Module",
     author: "Samsung Electronics, Inc.",
     methods: &copybit_module_methods,
        }
};

//------------- GLOBAL VARIABLE-------------------------------------------------//

#ifdef USE_SINGLE_INSTANCE
int ctx_created;
struct copybit_context_t *g_ctx;
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
#endif // USE_SINGLE_INSTANCE

//-----------------------------------------------------------------------------//

// Open a new instance of a copybit device using name
static int open_copybit(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = 0;

#ifdef USE_SINGLE_INSTANCE
    pthread_mutex_lock(&lock);
    if (ctx_created == 1) {
        *device = &g_ctx->device.common;
        status = 0;
        g_ctx->count++;
        pthread_mutex_unlock(&lock);
        return status;
    }
#endif // USE_SINGLE_INSTANCE

    struct copybit_context_t *ctx =
        (copybit_context_t *)malloc(sizeof(struct copybit_context_t));

#ifdef USE_SINGLE_INSTANCE
    g_ctx = ctx;
#endif // USE_SINGLE_INSTANCE

    memset(ctx, 0, sizeof(*ctx));

    ctx->device.common.tag     = HARDWARE_DEVICE_TAG;
    ctx->device.common.version = 0;
    ctx->device.common.module  = (struct hw_module_t *)module;
    ctx->device.common.close   = close_copybit;
    ctx->device.set_parameter  = set_parameter_copybit;
    ctx->device.get            = get_parameter_copybit;
    ctx->device.blit           = blit_copybit;
    ctx->device.stretch        = stretch_copybit;
    ctx->s5p_pp                = NULL;
    ctx->mAlpha                = S3C_ALPHA_NOP;
    ctx->mFlags                = 0;

    // get width * height for decide virtual frame size..
    char const * const device_template[] = {
        "/dev/graphics/fb%u",
        "/dev/fb%u",
        0 };
    int fb_fd = -1;
    int i=0;
    char fb_name[64];
    struct fb_var_screeninfo info;

    while ((fb_fd==-1) && device_template[i]) {
        snprintf(fb_name, 64, device_template[i], DEFAULT_FB_NUM);
        fb_fd = open(fb_name, O_RDONLY, 0);
        if (0 > fb_fd)
            LOGE("%s:: %s errno=%d\n", __func__, fb_name, errno);
        i++;
    }

    if (0 < fb_fd && ioctl(fb_fd, FBIOGET_VSCREENINFO, &info) >= 0) {
        ctx->s3c_fb.width  = info.xres;
        ctx->s3c_fb.height = info.yres;
    } else {
        LOGE("%s::%d = open(%s) fail or FBIOGET_VSCREENINFO fail.. So we can not do..\n",
                __func__, fb_fd, fb_name);
        status = -ENODEV;
    }

    if(0 < fb_fd)
        close(fb_fd);

    if(createPP(&ctx->s5p_pp) < 0) {
        LOGE("%s::createPP fail\n", __func__);
        status = -EINVAL;
    }

    if (status == 0) {
        *device = &ctx->device.common;

#ifdef USE_SINGLE_INSTANCE
        ctx->count = 1;
        ctx_created = 1;
#endif // USE_SINGLE_INSTANCE
    } else {
        close_copybit(&ctx->device.common);
    }

#ifdef USE_SINGLE_INSTANCE
    pthread_mutex_unlock(&lock);
#endif // USE_SINGLE_INSTANCE

    return status;
}

// Close the copybit device
static int close_copybit(struct hw_device_t *dev)
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int ret = 0;

#ifdef USE_SINGLE_INSTANCE
    pthread_mutex_lock(&lock);
    ctx->count--;

    if (0 >= ctx->count)
#else
        if (ctx)
#endif
        {
            ctx->s3c_fb.width = 0;
            ctx->s3c_fb.height = 0;

            if(destroyPP(&ctx->s5p_pp) < 0) {
                LOGE("%s::destroyPP fail\n", __func__);
                ret = -1;
            }

            free(ctx);

#ifdef USE_SINGLE_INSTANCE
            ctx_created = 0;
            g_ctx = NULL;
#endif // USE_SINGLE_INSTANCE
        }

#ifdef USE_SINGLE_INSTANCE
    pthread_mutex_unlock(&lock);
#endif

    return ret;
}

// min of int a, b
static inline int min(int a, int b)
{
    return (a<b) ? a : b;
}

// max of int a, b
static inline int max(int a, int b)
{
    return (a>b) ? a : b;
}

// scale each parameter by mul/div. Assume div isn't 0
static inline void MULDIV(uint32_t *a, uint32_t *b, int mul, int div)
{
    if (mul != div) {
        *a = (mul * *a) / div;
        *b = (mul * *b) / div;
    }
}

// Determine the intersection of lhs & rhs store in out
static void intersect(struct copybit_rect_t *out,
        const struct copybit_rect_t *lhs,
        const struct copybit_rect_t *rhs)
{
    out->l = max(lhs->l, rhs->l);
    out->t = max(lhs->t, rhs->t);
    out->r = min(lhs->r, rhs->r);
    out->b = min(lhs->b, rhs->b);
}

static void set_image(s5p_img *img,
        const struct copybit_image_t *rhs)
{
    // this code is specific to MSM7K
    // Need to modify
    struct private_handle_t* hnd = (struct private_handle_t*)rhs->handle;

    img->width  = rhs->w;
    img->height = rhs->h;
    img->format = rhs->format;
    if (0 != (uint32_t)rhs->base)
        img->base = (uint32_t)rhs->base;
    else
#ifdef BOARD_SUPPORT_SYSMMU
        img->base = hnd->ump_id;
#else
    img->base = hnd->base;
#endif

    if (hnd) {
        img->offset    = hnd->offset;
        img->memory_id = hnd->fd;
    } else {
        img->offset    = 0;
        img->memory_id = 0;
    }
}

// setup rectangles
static void set_rects(struct copybit_context_t *dev,
        s5p_img  *src_img,
        s5p_rect *src_rect,
        s5p_rect *dst_rect,
        const struct copybit_rect_t *src,
        const struct copybit_rect_t *dst,
        const struct copybit_rect_t *scissor)
{
    struct copybit_rect_t clip;
    intersect(&clip, scissor, dst);

    dst_rect->x  = clip.l;
    dst_rect->y  = clip.t;
    dst_rect->w  = clip.r - clip.l;
    dst_rect->h  = clip.b - clip.t;

    uint32_t W, H;
    if (dev->mFlags & COPYBIT_TRANSFORM_ROT_90) {
        src_rect->x = (clip.t - dst->t) + src->t;
        src_rect->y = (dst->r - clip.r) + src->l;
        src_rect->w = (clip.b - clip.t);
        src_rect->h = (clip.r - clip.l);
        W = dst->b - dst->t;
        H = dst->r - dst->l;
    } else {
        src_rect->x  = (clip.l - dst->l) + src->l;
        src_rect->y  = (clip.t - dst->t) + src->t;
        src_rect->w  = (clip.r - clip.l);
        src_rect->h  = (clip.b - clip.t);
        W = dst->r - dst->l;
        H = dst->b - dst->t;
    }
    MULDIV(&src_rect->x, &src_rect->w, src->r - src->l, W);
    MULDIV(&src_rect->y, &src_rect->h, src->b - src->t, H);
    if (dev->mFlags & COPYBIT_TRANSFORM_FLIP_V) {
        src_rect->y = src_img->height - (src_rect->y + src_rect->h);
    }
    if (dev->mFlags & COPYBIT_TRANSFORM_FLIP_H) {
        src_rect->x = src_img->width  - (src_rect->x + src_rect->w);
    }
}

// Set a parameter to value
static int set_parameter_copybit(struct copybit_device_t *dev, int name, int value)
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int status = 0;
    if (ctx) {
        switch(name) {
        case COPYBIT_ROTATION_DEG:
            switch (value) {
            case 0:
                ctx->mFlags &= ~0x7;
                break;
            case 90:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_90;
                break;
            case 180:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_180;
                break;
            case 270:
                ctx->mFlags &= ~0x7;
                ctx->mFlags |= COPYBIT_TRANSFORM_ROT_270;
                break;
            default:
                LOGE("%s::Invalid value for COPYBIT_ROTATION_DEG", __func__);
                status = -EINVAL;
                break;
            }
            break;
        case COPYBIT_PLANE_ALPHA:
            if (value < 0)      value = 0;
            if (value >= 256)   value = 255;
            ctx->mAlpha = value;
            break;
        case COPYBIT_DITHER:
            if (value == COPYBIT_ENABLE) {
                ctx->mFlags |= 0x8;
            } else if (value == COPYBIT_DISABLE) {
                ctx->mFlags &= ~0x8;
            }
            break;
        case COPYBIT_TRANSFORM:
            ctx->mFlags &= ~0x7;
            ctx->mFlags |= value & 0x7;
            break;
        default:
            status = -EINVAL;
            break;
        }
    } else {
        status = -EINVAL;
    }
    return status;
}

// Get a static info value
static int get_parameter_copybit(struct copybit_device_t *dev, int name)
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int value;
    if (ctx) {
        switch(name) {
        case COPYBIT_MINIFICATION_LIMIT:
            value = MAX_RESIZING_RATIO_LIMIT;
            break;
        case COPYBIT_MAGNIFICATION_LIMIT:
            value = MAX_RESIZING_RATIO_LIMIT;
            break;
        case COPYBIT_SCALING_FRAC_BITS:
            value = 32;
            break;
        case COPYBIT_ROTATION_STEP_DEG:
            value = 90;
            break;
        default:
            value = -EINVAL;
        }
    } else {
        value = -EINVAL;
    }
    return value;
}

// do a stretch blit type operation
static int stretch_copybit(struct copybit_device_t *dev,
        struct copybit_image_t  const *dst,
        struct copybit_image_t  const *src,
        struct copybit_rect_t   const *dst_rect,
        struct copybit_rect_t   const *src_rect,
        struct copybit_region_t const *region)
{
    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;
    int status = 0;
    nsecs_t   before1, after1;

    switch(src->format) {
    // COPYBIT_FORMAT_RGB_565 will be supported.
    //case COPYBIT_FORMAT_RGB_565:
    case COPYBIT_FORMAT_RGB_888:
    case COPYBIT_FORMAT_RGBA_8888:
    case COPYBIT_FORMAT_RGBX_8888:
    case COPYBIT_FORMAT_BGRA_8888:
    case COPYBIT_FORMAT_RGBA_5551:
    case COPYBIT_FORMAT_RGBA_4444:
        return -1;
    }

    if (ctx) {
        const struct copybit_rect_t bounds = { 0, 0, dst->w, dst->h };
        s5p_img src_img;
        s5p_img dst_img;
        s5p_rect src_work_rect;
        s5p_rect dst_work_rect;

        struct copybit_rect_t clip;
        int count = 0;
        status = 0;

        while ((status == 0) && region->next(region, &clip)) {
            count++;
            intersect(&clip, &bounds, &clip);
            set_image(&src_img, src);
            set_image(&dst_img, dst);
            set_rects(ctx, &src_img,&src_work_rect, &dst_work_rect,
                    src_rect, dst_rect, &clip);

#ifdef	MEASURE_STRETCH_DURATION
            before1  = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
            int ret;
            ret = doPP(ctx->s5p_pp, ctx->mFlags, ctx->mAlpha,
                    &src_img, &src_work_rect, &dst_img, &dst_work_rect);
            if(ret < 0) {
                LOGE("%s::sec_stretch faild(%d)\n", __func__, ret);
                status = -EINVAL;
            }
#ifdef	MEASURE_STRETCH_DURATION
            after1  = systemTime(SYSTEM_TIME_MONOTONIC);
            LOGD("sec_stretch[%d]: duration=%lld",
                    count, ns2us(after1-before1));
#endif
        }
    } else {
        status = -EINVAL;
    }

#ifdef USE_SINGLE_INSTANCE
    // re-initialize
    ctx->mAlpha                = S3C_ALPHA_NOP;
    ctx->mFlags                = 0;
#endif

    return status;
}

// Perform a blit type operation
static int blit_copybit(struct copybit_device_t *dev,
        struct copybit_image_t  const *dst,
        struct copybit_image_t  const *src,
        struct copybit_region_t const *region)
{
    int ret = 0;

    struct copybit_rect_t dr = { 0, 0, dst->w, dst->h };
    struct copybit_rect_t sr = { 0, 0, src->w, src->h };

    struct copybit_context_t* ctx = (struct copybit_context_t*)dev;

    nsecs_t   before, after;

#ifdef MEASURE_BLIT_DURATION
    before  = systemTime(SYSTEM_TIME_MONOTONIC);
    LOGD("[%lld us] blit_copybit", ns2us(before));
#endif

    ret = stretch_copybit(dev, dst, src, &dr, &sr, region);

#ifdef MEASURE_BLIT_DURATION
    after  = systemTime(SYSTEM_TIME_MONOTONIC);
    LOGD("[%lld us] blit_copybit-end (duration=%lld us)",
            ns2us(after), ns2us(after-before));
#endif

    return ret;
}
