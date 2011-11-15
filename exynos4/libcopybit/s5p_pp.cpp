/*
 * copyright@ samsung electronics co. ltd
 *
 * licensed under the apache license, version 2.0 (the "license");
 * you may not use this file except in compliance with the license.
 * you may obtain a copy of the license at
 *
 *      http://www.apache.org/licenses/license-2.0
 *
 * unless required by applicable law or agreed to in writing, software
 * distributed under the license is distributed on an "as is" basis,
 * without warranties or conditions of any kind, either express or implied.
 * see the license for the specific language governing permissions and
 * limitations under the license.
 */


#include <cutils/log.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <hardware/copybit.h>
#include <linux/android_pmem.h>
#include "s5p_pp.h"
#include "utils/Timers.h"
#include "s3c_lcd.h"

#include "android_pmem_s5p.h"

//#define MEASURE_PP_DURATION
//
#define S3C_MEM_DEV_NAME     "/dev/s3c-mem"
#define	PMEM_DEVICE_DEV_NAME "/dev/pmem_gpu1"

//---------------------- General Functions --------------------------------//
s5p_pp_addr_t getSrcPhyAddr(s5p_pp_t *p_s5p_pp, s5p_img *src_img);
int getDstPhyAddr(s5p_pp_t *p_s5p_pp, s5p_img *dst_img, int *dst_memcpy_flag,
        unsigned int *dst_frame_size);
static inline unsigned int getFrameSize(int colorformat, int width,
        int height);
static int copyRGBFrame(
        unsigned char * src_virt_addr, unsigned char * dst_virt_addr,
        unsigned int full_width, unsigned int full_height,
        unsigned int start_x, unsigned int start_y,
        unsigned int width, unsigned int height, int copybit_color_space);

//--------------------- Functions for memory management -------------------//
static int createMem (s3c_mem_t *arr_s3c_mem, unsigned int arr_num,
        unsigned int memory_size);
static int destroyMem(s3c_mem_t *arr_s3c_mem, unsigned int arr_num);
static int checkMem(s3c_mem_t *arr_s3c_mem, unsigned int arr_num,
        unsigned int requested_size);

#ifdef HW_PMEM_USE
static int initPmem (s5p_pp_t* p_s5p_pp);
static int destroyPmem(s5p_pp_t* p_s5p_pp);
static int checkPmem(s5p_pp_t* p_s5p_pp, unsigned int requested_size,
        unsigned int is_for_dst);
#endif

//--------------------- Functions for FIMC -------------------------------//
static bool createFIMC(SecFimc* sec_fimc);
static bool destroyFIMC (SecFimc* sec_fimc);
static int doFIMC (SecFimc* sec_fimc,
        s5p_pp_addr_t src_phys_addr, s5p_img *src_img, s5p_rect *src_rect,
        unsigned int dst_phys_addr, s5p_img *dst_img, s5p_rect *dst_rect,
        int rotate_flag);
static inline int rotateValueCopybit2FIMC(unsigned char flags);
static inline int colorFormatCopybit2FIMC(int format);

//--------------------- Functions for G2D --------------------------------//
#ifdef HW_G2D_USE
static int createG2D (sec_g2d_t *s3c_g2d);
static int destroyG2D(sec_g2d_t *s3c_g2d);
static int doG2D     (sec_g2d_t *s3c_g2d,
        unsigned int src_phys_addr, s5p_img *src_img, s5p_rect *src_rect,
        unsigned int dst_phys_addr, s5p_img *dst_img, s5p_rect *dst_rect,
        int rotate_value, int alpha_value);
static inline int colorFormatCopybit2G2D(int format);
#ifdef USE_FIMGAPI
static inline int colorFormatCopybit2FimgApi(int format);
#endif
static inline int rotateValueCopybit2G2D(unsigned char flags);
#endif

//--------------------- global variables ----------------------------------//
int pp_created;
s5p_pp_t g_s5p_pp;

int createPP(void **p_s5p_pp)
{
    int status = 0;
    LOGV("%s", __func__);
    if (pp_created == 1) {
        p_s5p_pp = (void **)&g_s5p_pp;
        return status;
    }

    s5p_pp_t *s5p_pp = &g_s5p_pp;

#ifdef HW_PMEM_USE
    initPmem(s5p_pp);
#endif

    //set PP
    if (!createFIMC(&(g_s5p_pp.sec_fimc))) {
        LOGE("%s::createPP fail\n", __func__);
        status = -EINVAL;
        return status;
    }

#ifdef HW_G2D_USE
#ifndef USE_FIMGAPI
    {
        //set g2d
        if (createG2D(&(g_s5p_pp.s5p_g2d)) < 0) {
            LOGE("%s::createG2D fail\n", __func__);
            status = -EINVAL;
            return status;
        }
    }
#endif
#endif
    pp_created = 1;
    *p_s5p_pp = &g_s5p_pp;
    return status;
}

int destroyPP(void **p_s5p_pp)
{
    LOGV("%s", __func__);
    s5p_pp_t *s5p_pp =(s5p_pp_t *)*p_s5p_pp;
    int ret = 0;

    if (pp_created != 1) {
        LOGE("%s:: PP is not created\n", __func__);
        return -1;
    }
#ifdef HW_G2D_USE
#ifndef USE_FIMGAPI
    {
        if (destroyG2D(&(s5p_pp->s5p_g2d)) < 0) {
            LOGE("%s::destroyG2D fail\n", __func__);
            ret = -1;
        }
    }
#endif
#endif

    if (destroyFIMC(&(s5p_pp->sec_fimc)) < 0) {
        LOGE("%s::destroyFIMC fail\n", __func__);
        ret = -1;
    }

    if (destroyMem(s5p_pp->s3c_mem, NUM_OF_MEMORY_OBJECT) < 0) {
        LOGE("%s::destroyMem fail\n", __func__);
        ret = -1;
    }

#ifdef HW_PMEM_USE
    if (destroyPmem(s5p_pp) < 0) {
        LOGE("%s::destroyPmem fail\n", __func__);
        ret = -1;
    }
#endif

    p_s5p_pp = NULL;

    pp_created = 0;

    return ret;
}

int doPP(void *p_s5p_pp, int copybit_rotate_flag, int copybit_alpha_flag,
        s5p_img *src_img, s5p_rect *src_rect,
        s5p_img *dst_img, s5p_rect *dst_rect)
{
    LOGV("%s", __func__);
    s5p_pp_t *s5p_pp = (s5p_pp_t *)p_s5p_pp;
#ifdef HW_G2D_USE
    sec_g2d_t *s5p_g2d  = &(s5p_pp->s5p_g2d);
#endif

    s3c_mem_t *s3c_mem_src  = &(s5p_pp->s3c_mem[0]);
    s3c_mem_t *s3c_mem_dst  = &(s5p_pp->s3c_mem[1]);

    s5p_pp_addr_t src_phys_addr;
    unsigned int dst_phys_addr  = 0;
    int          rotate_value   = 0;
    unsigned int thru_g2d_path  = 0;
    int          flag_force_memcpy = 0;
    unsigned int dst_frame_size = 0;

    nsecs_t before, after;

    // 1 : source address and size
    // ask this is fb
    src_phys_addr = getSrcPhyAddr(s5p_pp, src_img);
    if (0 == (src_phys_addr.addr_y))
        return -1;

    // 2 : destination address and size
    // ask this is fb
    if (0 == (dst_phys_addr = getDstPhyAddr(s5p_pp, dst_img,
                    &flag_force_memcpy, &dst_frame_size)))
        return -2;

    // should do 3, 4 step
    // 3 : select HW IP path to use
    // 4 : Do post process according to the HW IP path
#ifdef HW_G2D_USE
    rotate_value = rotateValueCopybit2G2D(copybit_rotate_flag);
#endif

    // FIMC handles YUV format
    if (COPYBIT_FORMAT_YCbCr_422_SP <= src_img->format) {
        unsigned int thru_g2d_path = 0;
        int32_t src_color_space;
        int32_t dst_color_space;

        // check whether fimc supports the src format
        if (0 > (src_color_space = colorFormatCopybit2FIMC(src_img->format)))
            return -3;

        if (0 > (dst_color_space = colorFormatCopybit2FIMC(dst_img->format)))
            return -4;

        if (dst_img->format < COPYBIT_FORMAT_RGBA_4444) {
            // check the path to handle the dst RGB data
            if (copybit_alpha_flag < 255)
                thru_g2d_path = 1;

            if (thru_g2d_path)
                LOGV("%s:: copybit use G2D ", __func__);

            if (0 == thru_g2d_path) {
                // PATH : src img -> FIMC -> ext output dma (FB or mem)
#ifdef MEASURE_PP_DURATION
                before  = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
                if (doFIMC(&(s5p_pp->sec_fimc),
                            src_phys_addr, src_img, src_rect,
                            dst_phys_addr, dst_img, dst_rect,
                            copybit_rotate_flag) < 0) {
                    return -5;
                }

#ifdef MEASURE_PP_DURATION
                after  = systemTime(SYSTEM_TIME_MONOTONIC);
                LOGD("[fimc->fb] doPP: duration=%lld (dst_phys_addr=%x)",
                        ns2us(after-before), dst_phys_addr);
#endif
            } else {
#ifdef HW_G2D_USE
                // PATH : srg img -> FIMC -> internal output dma -> G2D
                // -> dst (FB or mem)
                s5p_img  temp_img;
                s5p_rect temp_rect;

                if (dst_img->format == COPYBIT_FORMAT_RGBA_8888)
                    temp_img.format = COPYBIT_FORMAT_RGBX_8888;
                else
                    temp_img.format = dst_img->format;

                temp_img.memory_id = dst_img->memory_id;
                temp_img.offset    = dst_img->offset;
                temp_img.base      = dst_img->base;
                temp_rect.x = 0;
                temp_rect.y = 0;

                temp_img.width  = dst_img->width;
                temp_img.height = dst_img->height;
                temp_rect.w     = dst_rect->w;
                temp_rect.h     = dst_rect->h;

#ifdef MEASURE_PP_DURATION
                before  = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
                if (doFIMC(&(s5p_pp->sec_fimc),
                            src_phys_addr, src_img, src_rect,
                            (unsigned int)s5p_pp->sec_fimc.getFimcRsrvedPhysMemAddr(), 
                            &temp_img, &temp_rect, copybit_rotate_flag) < 0) {
                    return -6;
                }
#ifdef MEASURE_PP_DURATION
                after  = systemTime(SYSTEM_TIME_MONOTONIC);
                LOGD("doPP: duration=%lld", ns2us(after-before));
#endif

#ifdef MEASURE_PP_DURATION
                before  = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
                if (doG2D(s5p_g2d, s5p_pp->sec_fimc.getFimcRsrvedPhysMemAddr(),
                            &temp_img, &temp_rect,
                            dst_phys_addr, dst_img, dst_rect,
                            G2D_ROT_0, copybit_alpha_flag) < 0) {
                    return -7;
                }
#ifdef MEASURE_PP_DURATION
                after  = systemTime(SYSTEM_TIME_MONOTONIC);
                LOGD("doG2D: duration=%lld", ns2us(after-before));
#endif
#else
                return -8;
#endif
            }
        } else {
            if (doFIMC(&(s5p_pp->sec_fimc),
                        src_phys_addr, src_img, src_rect,
                        dst_phys_addr, dst_img, dst_rect,
                        copybit_rotate_flag) < 0)
                return -9;
        }
    } else {
#ifdef HW_G2D_USE
#ifdef MEASURE_PP_DURATION
        before  = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
        if (doG2D(s5p_g2d, src_phys_addr.addr_y, src_img, src_rect,
                    dst_phys_addr, dst_img, dst_rect, rotate_value,
                    copybit_alpha_flag) < 0)
            return -10;

#ifdef MEASURE_PP_DURATION
        after  = systemTime(SYSTEM_TIME_MONOTONIC);
        LOGD("doG2D: duration=%lld", ns2us(after-before));
#endif
#else
        if (doFIMC(&(s5p_pp->sec_fimc),
                    src_phys_addr, src_img, src_rect,
                    dst_phys_addr, dst_img, dst_rect,
                    copybit_rotate_flag) < 0) {
            return -11;
        }
#endif
    }

#ifdef	HW_PMEM_USE
    if (flag_force_memcpy >= 0) {
        struct pmem_region region;

        if (flag_force_memcpy == 0) {
            region.offset =
                (unsigned long)s5p_pp->sec_pmem.sec_pmem_alloc.dst_virt_addr;
            region.len    = s5p_pp->sec_pmem.sec_pmem_alloc.dst_size;
        } else {
            region.offset =
                (unsigned long)s3c_mem_dst->mem_alloc_info.vir_addr;
            region.len    = s3c_mem_dst->mem_alloc_info.size;
        }
        ioctl(s5p_pp->sec_pmem.pmem_master_fd, PMEM_CACHE_INV, &region);

        if (flag_force_memcpy == 0)
            memcpy((void*)((unsigned int)dst_img->base),
                    (void *)s5p_pp->sec_pmem.sec_pmem_alloc.dst_virt_addr,
                    dst_frame_size);
        else
            memcpy((void*)((unsigned int)dst_img->base),
                    (void *)s3c_mem_dst->mem_alloc_info.vir_addr,
                    dst_frame_size);
    }
#endif

    return 0;
}


s5p_pp_addr_t getSrcPhyAddr(s5p_pp_t *p_s5p_pp, s5p_img *src_img)
{
    LOGV("%s", __func__);
    unsigned int src_virt_addr  = 0;
    unsigned int src_frame_size = 0;
    struct pmem_region region;
    s3c_fb_next_info_t fb_info;

    s5p_pp_addr_t src_phys_addr;
    src_phys_addr.addr_y = 0;
    src_phys_addr.addr_cb = 0;
    src_phys_addr.addr_cr = 0;

    s3c_mem_t *s3c_mem_src = &(p_s5p_pp->s3c_mem[0]);
#ifdef	HW_PMEM_USE
    sec_pmem_t *sec_pm = &(p_s5p_pp->sec_pmem);
#endif

    switch(src_img->format) {
    case COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP:
    case COPYBIT_FORMAT_CUSTOM_YCrCb_420_SP:
        // sw workaround for video content zero copy
        memcpy(&(src_phys_addr.addr_y),
                (void*)((unsigned int)src_img->base), 4);
        memcpy(&(src_phys_addr.addr_cb),
                (void*)((unsigned int)src_img->base+4), 4);
        break;
        break;

    case COPYBIT_FORMAT_CUSTOM_YCbCr_422_I:
    case COPYBIT_FORMAT_CUSTOM_CbYCr_422_I:
        // sw workaround for camera capture zero copy
        memcpy(&(src_phys_addr.addr_y),
                (void*)((unsigned int)src_img->base + src_img->offset), 4);
        break;

    default:
        // check the pmem case
        if (ioctl(src_img->memory_id, PMEM_GET_PHYS, &region) >= 0) {
            src_phys_addr.addr_y =
                (unsigned int)region.offset + src_img->offset;
        }
        // check the fb case
        else if (ioctl(src_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0) {
            src_phys_addr.addr_y = fb_info.phy_start_addr + src_img->offset;
        } else {
            // copy
            src_frame_size = getFrameSize(src_img->format, src_img->width, src_img->height);

            if (src_frame_size == 0) {
                LOGD("%s::getFrameSize fail\n", __func__);
                return src_phys_addr;
            }
#ifdef	HW_PMEM_USE
            if (0 <= checkPmem(p_s5p_pp, src_frame_size, 0)) {
                src_virt_addr = sec_pm->sec_pmem_alloc.src_virt_addr;
                src_phys_addr.addr_y = sec_pm->sec_pmem_alloc.src_phys_addr;
            }
            else
#endif
            {
                if (0 <= checkMem(s3c_mem_src, 1, src_frame_size)) {
                    src_virt_addr = s3c_mem_src->mem_alloc_info.vir_addr;
                    src_phys_addr.addr_y = s3c_mem_src->mem_alloc_info.phy_addr;
                } else {
                    LOGD("%s::check_mem fail\n", __func__);
                    return src_phys_addr;
                }
            }
#ifdef MEASURE_PP_DURATION
            before = systemTime(SYSTEM_TIME_MONOTONIC);
#endif

            memcpy((void *)src_virt_addr,
                    (void*)((unsigned int)src_img->base), src_frame_size);

#ifdef MEASURE_PP_DURATION
            after  = systemTime(SYSTEM_TIME_MONOTONIC);
            LOGD("src_memcpy: duration=%lld (src_frame_size=%d, id=%d)",
                    ns2us(after-before), src_frame_size, src_img->memory_id);
#endif
        }
    }

    return src_phys_addr;
}

int getDstPhyAddr(s5p_pp_t *p_s5p_pp, s5p_img *dst_img, int *dst_memcpy_flag,
        unsigned int *dst_frame_size)
{
    LOGV("%s", __func__);
    // 2 : destination address and size
    // ask this is fb
    int dst_phys_addr = 0;
    unsigned int ui_dst_frame_size = 0;
    *dst_frame_size = 0;
    s3c_mem_t *s3c_mem_dst = &(p_s5p_pp->s3c_mem[1]);

    struct pmem_region region;
    s3c_fb_next_info_t fb_info;

    *dst_memcpy_flag = -1;

#ifdef	HW_PMEM_USE
    sec_pmem_t *sec_pm = &(p_s5p_pp->sec_pmem);
#endif

    if (0 == dst_img->memory_id && 0 != dst_img->base) {
        dst_phys_addr = dst_img->base;
    } else {
        if (ioctl(dst_img->memory_id, S3C_FB_GET_CURR_FB_INFO, &fb_info) >= 0) {
            dst_phys_addr = fb_info.phy_start_addr + dst_img->offset;
        }
#ifdef ADJUST_PMEM
        else if (ioctl(dst_img->memory_id, PMEM_GET_PHYS, &region) >= 0) {
            dst_phys_addr = (unsigned int)region.offset + dst_img->offset;
        }
#endif // ADJUST_PMEM
#ifdef BOARD_SUPPORT_SYSMMU
        else {
            /* RyanJung modify for testing 2010-11-17*/
            dst_phys_addr = (int)dst_img->memory_id;
        }
#else
        else {
            ui_dst_frame_size = getFrameSize(dst_img->format,
                    dst_img->width, dst_img->height);

            if (ui_dst_frame_size == 0) {
                LOGE("%s::getFrameSize fail \n", __func__);
                return 0;
            }

#ifdef	HW_PMEM_USE
            if (0 <= checkPmem(p_s5p_pp, ui_dst_frame_size, 1)) {
                dst_phys_addr = sec_pm->sec_pmem_alloc.dst_phys_addr;
                *dst_memcpy_flag = 0;
            } else
#endif
            {
                if (0 > checkMem(s3c_mem_dst, 1, ui_dst_frame_size)) {
                    LOGD("%s::check_mem for destination fail \n", __func__);
                    return 0;
                }

                dst_phys_addr = s3c_mem_dst->mem_alloc_info.phy_addr;
                *dst_memcpy_flag = 1;
            }
            // memcpy trigger...
            *dst_frame_size = ui_dst_frame_size;
        }
#endif
    }
    return dst_phys_addr;
}

int checkMem(s3c_mem_t *arr_s3c_mem, unsigned int arr_num,
        unsigned int requested_size)
{
    LOGV("%s", __func__);
    if (0 == arr_s3c_mem->mem_alloc_info.vir_addr) {
        // memory is not allocated. need to allocate
        if (createMem(arr_s3c_mem, arr_num, requested_size) < 0) {
            LOGE("%s::createMem fail (size=%d)\n", __func__, requested_size);
            return -1;
        }
    } else if ((unsigned int)arr_s3c_mem->mem_alloc_info.size < requested_size) {
        // allocated mem is smaller than the requested.
        // need to free previously-allocated mem and reallocate

        if (destroyMem(arr_s3c_mem, arr_num) < 0) {
            LOGE("%s::destroyMem fail\n", __func__);
            return -1;
        }

        if (createMem(arr_s3c_mem, arr_num, requested_size) < 0) {
            LOGE("%s::createMem fail (size=%d)\n", __func__, requested_size);
            return -1;
        }
    }

    return 0;
}

static int createMem(s3c_mem_t *arr_s3c_mem, unsigned int arr_num,
        unsigned int memory_size)
{
    LOGV("%s", __func__);
    unsigned int i = 0;

    for(i = 0; i < arr_num; i++) {
        s3c_mem_t  * s3c_mem = arr_s3c_mem + i;

        if (s3c_mem->dev_fd == 0)
            s3c_mem->dev_fd = open(S3C_MEM_DEV_NAME, O_RDWR);
        if (s3c_mem->dev_fd < 0) {
            LOGE("%s::open(%s) fail(%s)\n", __func__, S3C_MEM_DEV_NAME,
                    strerror(errno));
            s3c_mem->dev_fd = 0;
            return -1;
        }

        if (0 == memory_size)
            continue;

        s3c_mem->mem_alloc_info.size = memory_size;

        if (ioctl(s3c_mem->dev_fd, S3C_MEM_CACHEABLE_ALLOC,
                    &s3c_mem->mem_alloc_info) < 0) {
            LOGE("%s::S3C_MEM_ALLOC(size : %d)  fail\n",
                    __func__, s3c_mem->mem_alloc_info.size);
            return -1;
        }
    }

    return 0;
}

static int destroyMem(s3c_mem_t *arr_s3c_mem, unsigned int arr_num)
{
    LOGV("%s", __func__);
    unsigned int i = 0;
    for(i = 0; i < arr_num; i++) {
        s3c_mem_t  * s3c_mem = &arr_s3c_mem[i];

        if (0 < s3c_mem->dev_fd) {
            if (ioctl(s3c_mem->dev_fd, S3C_MEM_FREE, &s3c_mem->mem_alloc_info) < 0) {
                LOGE("%s::S3C_MEM_FREE fail\n", __func__);
                return -1;
            }

            close(s3c_mem->dev_fd);
            s3c_mem->dev_fd = 0;
        }
        s3c_mem->mem_alloc_info.phy_addr = 0;
        s3c_mem->mem_alloc_info.vir_addr = 0;
    }

    return 0;
}

#ifdef	HW_PMEM_USE
static int initPmem(s5p_pp_t* p_s5p_pp)
{
    LOGV("%s", __func__);
    int	   master_fd, err = 0;
    void   *base;
    size_t size;
    struct pmem_region region;
    sec_pmem_t *pm = &(p_s5p_pp->sec_pmem);

    master_fd = open(PMEM_DEVICE_DEV_NAME, O_RDWR, 0);
    if (master_fd < 0) {
        pm->pmem_master_fd = -1;
        if (EACCES == errno) {
            LOGE("%s::open(%s) fail(%s)\n",
                    __func__, (char *)PMEM_DEVICE_DEV_NAME, strerror(EACCES));
            return 0;
        } else {
            LOGE("%s::open(%s) fail(%s)\n",
                    __func__, (char *)PMEM_DEVICE_DEV_NAME, strerror(errno));
            return -errno;
        }
    }

    if (ioctl(master_fd, PMEM_GET_TOTAL_SIZE, &region) < 0) {
        LOGE("PMEM_GET_TOTAL_SIZE failed, limp mode");
        size = 10<<20;
    } else {
        size = region.len;
    }

    base = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, master_fd, 0);
    if (base == MAP_FAILED) {
        LOGE("[%s] mmap failed : %d (%s)", __func__, errno, strerror(errno));
        err = -errno;
        base = 0;
        close(master_fd);
        master_fd = -1;
    }

    if (ioctl(master_fd, PMEM_GET_PHYS, &region) < 0) {
        LOGE("PMEM_GET_PHYS failed, limp mode");
    } else {
        pm->sec_pmem_alloc.dst_virt_addr = (unsigned int)base;
        pm->sec_pmem_alloc.dst_phys_addr = region.offset;
        pm->sec_pmem_alloc.dst_size	 = 2<<20;      // 2M for dstination
        pm->sec_pmem_alloc.src_virt_addr =
            (unsigned int)base + pm->sec_pmem_alloc.dst_size;
        pm->sec_pmem_alloc.src_phys_addr =
            region.offset + pm->sec_pmem_alloc.dst_size;
        pm->sec_pmem_alloc.src_size	 = size - pm->sec_pmem_alloc.dst_size;

        LOGV("src_virt_addr: 0x%8x, src_phys_addr: 0x%8x,"
                "dst_virt_addr: 0x%8x, dst_phys_addr: 0x%8x",
                pm->sec_pmem_alloc.src_virt_addr,
                pm->sec_pmem_alloc.src_phys_addr,
                pm->sec_pmem_alloc.dst_virt_addr,
                pm->sec_pmem_alloc.dst_phys_addr);
    }

    pm->pmem_master_fd 	 = master_fd;
    pm->pmem_master_base = base;
    pm->pmem_total_size  = size;

    return err;
}

static int destroyPmem(s5p_pp_t* p_s5p_pp)
{
    LOGV("%s", __func__);
    sec_pmem_t *pm = &(p_s5p_pp->sec_pmem);

    if (0 <= pm->pmem_master_fd) {
        munmap(pm->pmem_master_base, pm->pmem_total_size);
        close(pm->pmem_master_fd);
        pm->pmem_master_fd = -1;
    }

    pm->pmem_master_base             = 0;
    pm->sec_pmem_alloc.src_virt_addr = 0;
    pm->sec_pmem_alloc.src_phys_addr = 0;
    pm->sec_pmem_alloc.src_size      = 0;
    pm->sec_pmem_alloc.dst_virt_addr = 0;
    pm->sec_pmem_alloc.dst_phys_addr = 0;
    pm->sec_pmem_alloc.dst_size      = 0;

    return 0;
}

int checkPmem(s5p_pp_t *p_s5p_pp, unsigned int requested_size,
        unsigned int is_for_dst)
{
    LOGV("%s", __func__);
    sec_pmem_t	*pm	= &(p_s5p_pp->sec_pmem);

    if (is_for_dst) {
        if (0 < pm->sec_pmem_alloc.dst_virt_addr &&
                requested_size <= (unsigned int)(pm->sec_pmem_alloc.dst_size))
            return 0;
    } else {
        if (0 < pm->sec_pmem_alloc.src_virt_addr &&
                requested_size <= (unsigned int)(pm->sec_pmem_alloc.src_size))
            return 0;
    }

    return -1;
}
#endif

static inline unsigned int getFrameSize(int colorformat, int width, int height)
{
    unsigned int frame_size = 0;
    unsigned int size = width * height;

    switch(colorformat) {
    case COPYBIT_FORMAT_RGBA_8888:
    case COPYBIT_FORMAT_BGRA_8888:
        frame_size = width * height * 4;
        break;

    case COPYBIT_FORMAT_YCbCr_420_SP:
    case COPYBIT_FORMAT_YCrCb_420_SP:
    case COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP:
    case COPYBIT_FORMAT_CUSTOM_YCrCb_420_SP:
    case COPYBIT_FORMAT_YCbCr_420_P:
        frame_size = size + (2 * ( size / 4));
        break;

    case COPYBIT_FORMAT_RGB_565:
    case COPYBIT_FORMAT_RGBA_5551:
    case COPYBIT_FORMAT_RGBA_4444:
    case COPYBIT_FORMAT_YCbCr_422_I :
    case COPYBIT_FORMAT_YCbCr_422_SP :
    case COPYBIT_FORMAT_CUSTOM_YCbCr_422_I :
    case COPYBIT_FORMAT_CUSTOM_CbYCr_422_I :
        frame_size = width * height * 2;
        break;

    default :  // hell.~~
        LOGE("%s::no matching source colorformat(%d), width(%d), height(%d)\n",
                __func__, colorformat, width, height);
        frame_size = 0;
        break;
    }
    return frame_size;
}

static bool createFIMC(SecFimc* sec_fimc)
{
    LOGV("%s", __func__);
    return sec_fimc->create(SecFimc::FIMC_DEV1, FIMC_OVLY_NONE_SINGLE_BUF, 1);
}

static bool destroyFIMC(SecFimc* sec_fimc)
{
    LOGV("%s", __func__);

    bool ret = true;

    if (sec_fimc->flagCreate() == true)
        ret = sec_fimc->destroy();

    return ret;
}


static int doFIMC(SecFimc* sec_fimc,
        s5p_pp_addr_t src_phys_addr, s5p_img *src_img, s5p_rect *src_rect,
        unsigned int dst_phys_addr, s5p_img *dst_img, s5p_rect *dst_rect,
        int rotate_flag)
{
    LOGV("%s", __func__);
    unsigned int      frame_size = 0;

    int src_color_space, dst_color_space;
    int src_bpp, src_planes;

    struct fimc_buf fimc_src_buf;

    int rotate_value = rotateValueCopybit2FIMC(rotate_flag);
    nsecs_t before, after;

    // set post processor configuration
    src_color_space = colorFormatCopybit2FIMC(src_img->format);
    if (!sec_fimc->setSrcParams(src_img->width, src_img->height,
                src_rect->x, src_rect->y, &(src_rect->w), &(src_rect->h),
                src_color_space)) {
        LOGE("%s:: setSrcParms() failed", __func__);
        return -1;
    }
    if (!sec_fimc->setSrcPhyAddr(src_phys_addr.addr_y, src_phys_addr.addr_cb,
                src_phys_addr.addr_cr)) {
        LOGE("%s:: setSrcPhyAddr() failed", __func__);
        return -1;
    }

    dst_color_space = colorFormatCopybit2FIMC(dst_img->format);
    if (!sec_fimc->setRotVal(rotate_value)) {
        LOGE("%s:: setRotVal() failed", __func__);
        return -1;
    }
    if (!sec_fimc->setDstParams(dst_img->width, dst_img->height,
                dst_rect->x, dst_rect->y, &(dst_rect->w),
                &(dst_rect->h), dst_color_space)) {
        LOGE("%s:: setRotVal() failed", __func__);
        return -1;
    }
    if (!sec_fimc->setDstPhyAddr(dst_phys_addr)) {
        LOGE("%s:: setDstPhyAddr() failed", __func__);
        return -1;
    }

    if (!sec_fimc->handleOneShot()) {
        LOGE("%s:: handleOneShot() failed", __func__);
        return -1;
    }
    return 0;
}

static inline int rotateValueCopybit2FIMC(unsigned char flags)
{
    int rotate_flag = flags & 0x7;

    switch (rotate_flag) {
    case COPYBIT_TRANSFORM_ROT_90:  return 90;
    case COPYBIT_TRANSFORM_ROT_180: return 180;
    case COPYBIT_TRANSFORM_ROT_270: return 270;
    }
    return 0;
}

static inline int colorFormatCopybit2FIMC(int format)
{
    switch (format) {
    // rbg
    case COPYBIT_FORMAT_RGBA_8888: return V4L2_PIX_FMT_RGB32;
    case COPYBIT_FORMAT_RGBX_8888: return V4L2_PIX_FMT_RGB32;
    case COPYBIT_FORMAT_BGRA_8888: return V4L2_PIX_FMT_RGB32;
    case COPYBIT_FORMAT_RGB_565:   return V4L2_PIX_FMT_RGB565;

    // 422 / 420 2 plane
    case COPYBIT_FORMAT_YCbCr_422_SP: return V4L2_PIX_FMT_NV16;
    case COPYBIT_FORMAT_YCrCb_422_SP: return V4L2_PIX_FMT_NV61;
    case COPYBIT_FORMAT_YCbCr_420_SP: return V4L2_PIX_FMT_NV12;
    case COPYBIT_FORMAT_YCrCb_420_SP: return V4L2_PIX_FMT_NV21;

    // 422 / 420 3 plane
    case COPYBIT_FORMAT_YCbCr_422_P: return V4L2_PIX_FMT_YUV422P;
    case COPYBIT_FORMAT_YCbCr_420_P: return V4L2_PIX_FMT_YUV420;

    // 422 1 plane
    case COPYBIT_FORMAT_YCbCr_422_I: return V4L2_PIX_FMT_YUYV;
    case COPYBIT_FORMAT_CbYCrY_422_I:return V4L2_PIX_FMT_UYVY;

    // customed format
    case COPYBIT_FORMAT_CUSTOM_CbYCr_422_I:  return V4L2_PIX_FMT_UYVY;
    case COPYBIT_FORMAT_CUSTOM_YCbCr_422_I:  return V4L2_PIX_FMT_YUYV;
    case COPYBIT_FORMAT_CUSTOM_YCbCr_420_SP: return V4L2_PIX_FMT_NV12T;
    case COPYBIT_FORMAT_CUSTOM_YCrCb_420_SP: return V4L2_PIX_FMT_NV21;

    // unsupported format by fimc
    case COPYBIT_FORMAT_RGB_888:
    case COPYBIT_FORMAT_RGBA_5551:
    case COPYBIT_FORMAT_RGBA_4444:
    case COPYBIT_FORMAT_YCbCr_420_I:
    case COPYBIT_FORMAT_CbYCrY_420_I:
    default :
        LOGE("%s::not matched frame format : %d\n", __func__, format);
        break;
    }
    return -1;
}


#ifdef HW_G2D_USE
static int createG2D(sec_g2d_t *t_g2d)
{
    LOGV("%s", __func__);
    sec_g2d_t *sec_g2d = t_g2d;

    if (sec_g2d->dev_fd == 0)
        sec_g2d->dev_fd = open(SEC_G2D_DEV_NAME, O_RDONLY);

    if (sec_g2d->dev_fd < 0) {
        LOGE("%s::open(%s) fail\n", __func__, SEC_G2D_DEV_NAME);
        return -1;
    }

    return 0;
}

static int destroyG2D(sec_g2d_t *sec_g2d)
{
    LOGV("%s", __func__);
    if (sec_g2d->dev_fd != 0)
        close(sec_g2d->dev_fd);
    sec_g2d->dev_fd = 0;

    return 0;
}

static int doG2D(sec_g2d_t *t_g2d,
        unsigned int src_phys_addr, s5p_img *src_img, s5p_rect *src_rect,
        unsigned int dst_phys_addr, s5p_img *dst_img, s5p_rect *dst_rect,
        int rotate_value, int alpha_value)
{
    LOGV("%s", __func__);
#ifdef USE_FIMGAPI
    {
        FimgRect src_fimg_rect = {src_rect->x,
            src_rect->y,
            src_rect->w,
            src_rect->h,
            src_img->width,
            src_img->height,
            FimgApi::COLOR_FORMAT_RGB_565,
            src_phys_addr,
            NULL};

        FimgRect dst_fimg_rect = {dst_rect->x,
            dst_rect->y,
            dst_rect->w,
            dst_rect->h,
            dst_img->width,
            dst_img->height,
            FimgApi::COLOR_FORMAT_RGB_565,
            dst_phys_addr,
            NULL};

        FimgFlag fimg_flag = { rotate_value, alpha_value, 0, };

        src_fimg_rect.color_format =
            colorFormatCopybit2FimgApi(src_img->format);
        dst_fimg_rect.color_format =
            colorFormatCopybit2FimgApi(dst_img->format);

        if (src_fimg_rect.color_format == -1 ||
                dst_fimg_rect.color_format == -1) {
            LOGE("%s::colorFormatCopybit2FimgApi(src_img->format(%d)/"
                    "dst_img->format(%d)) fail\n",
                    __func__, src_img->format, dst_img->format);
            return -1;
        }

        if (stretchFimgApi(&src_fimg_rect, &dst_fimg_rect, &fimg_flag) < 0) {
            LOGE("%s::stretchFimgApi fail\n", __func__);
            return -1;
        }
    }
#else
    {
        sec_g2d_t *  sec_g2d = t_g2d;
        g2d_params * params  = &(sec_g2d->params);

        g2d_rect src_g2d_rect = {src_rect->x,
            src_rect->y,
            src_rect->w,
            src_rect->h,
            src_img->width,
            src_img->height,
            G2D_RGB_565,
            src_phys_addr,
            NULL};

        g2d_rect dst_g2d_rect = {dst_rect->x,
            dst_rect->y,
            dst_rect->w,
            dst_rect->h,
            dst_img->width,
            dst_img->height,
            G2D_RGB_565,
            dst_phys_addr,
            NULL};

        g2d_flag g2d_flag = { rotate_value, alpha_value, 0, };

        src_g2d_rect.color_format = colorFormatCopybit2G2D(src_img->format);
        dst_g2d_rect.color_format = colorFormatCopybit2G2D(dst_img->format);

        if (src_g2d_rect.color_format == -1 ||
                dst_g2d_rect.color_format == -1) {
            LOGE("%s::colorFormatCopybit2G2D(src_img->format(%d)/"
                    "dst_img->format(%d)) fail\n",
                    __func__, src_img->format, dst_img->format);
            return -1;
        }

        params->src_rect = &src_g2d_rect;
        params->dst_rect = &dst_g2d_rect;
        params->flag     = &g2d_flag;

        if (ioctl(sec_g2d->dev_fd, G2D_BLIT, params) < 0) {
            LOGE("%s::G2D_BLIT fail\n", __func__);
            return -1;
        }
    }
#endif // USE_FIMGAPI

    return 0;
}

static inline int colorFormatCopybit2G2D(int format)
{
    switch (format) {
    case COPYBIT_FORMAT_RGBA_8888 :  return G2D_ABGR_8888;
    case COPYBIT_FORMAT_RGBX_8888 :  return G2D_XBGR_8888;
    case COPYBIT_FORMAT_RGB_565   :  return G2D_RGB_565;
    case COPYBIT_FORMAT_BGRA_8888 :  return G2D_ARGB_8888;
    case COPYBIT_FORMAT_RGBA_5551 :  return G2D_XBGR_1555;
    case COPYBIT_FORMAT_RGBA_4444 :  return G2D_ABGR_4444;
    default :
        LOGE("%s::not matched frame format : %d\n", __func__, format);
        break;
    }
    return -1;
}

#ifdef USE_FIMGAPI
static inline int colorFormatCopybit2FimgApi(int format)
{
    switch (format) {
    case COPYBIT_FORMAT_RGBA_8888 :  return FimgApi::COLOR_FORMAT_ABGR_1555;
    case COPYBIT_FORMAT_RGBX_8888 :  return FimgApi::COLOR_FORMAT_XBGR_8888;
    case COPYBIT_FORMAT_RGB_565   :  return FimgApi::COLOR_FORMAT_RGB_565;
    case COPYBIT_FORMAT_BGRA_8888 :  return FimgApi::COLOR_FORMAT_ARGB_8888;
    case COPYBIT_FORMAT_RGBA_5551 :  return FimgApi::COLOR_FORMAT_XBGR_1555;
    case COPYBIT_FORMAT_RGBA_4444 :  return FimgApi::COLOR_FORMAT_ABGR_4444;
    default :
        LOGE("%s::not matched frame format : %d\n", __func__, format);
        break;
    }
    return -1;
}
#endif

static inline int rotateValueCopybit2G2D(unsigned char flags)
{
    int rotate_flag = flags & 0x7;

    switch (rotate_flag) {
    case COPYBIT_TRANSFORM_ROT_90: 	return G2D_ROT_0;
    case COPYBIT_TRANSFORM_ROT_180:     return G2D_ROT_180;
    case COPYBIT_TRANSFORM_ROT_270:     return G2D_ROT_270;
    case COPYBIT_TRANSFORM_FLIP_V:      return G2D_ROT_X_FLIP;
    case COPYBIT_TRANSFORM_FLIP_H:      return G2D_ROT_Y_FLIP;
    }

    return G2D_ROT_0;
}

#endif // HW_G2D_USE

// this func assumes G2D_RGB16, G2D_RGBA16, G2D_RGBA32 ...
static int copyRGBFrame(
        unsigned char * src_virt_addr, unsigned char * dst_virt_addr,
        unsigned int full_width, unsigned int full_height,
        unsigned int start_x, unsigned int start_y,
        unsigned int width, unsigned int height,
        int copybit_color_space)
{
    LOGV("%s", __func__);
    unsigned int x;
    unsigned int y;

    unsigned char * current_src_addr  = src_virt_addr;
    unsigned char * current_dst_addr  = dst_virt_addr;

    unsigned int full_frame_size  = 0;

    unsigned int real_full_width;
    unsigned int real_full_height;
    unsigned int real_start_x;
    unsigned int real_width;

    // Is the color format supported ??
    if (copybit_color_space != COPYBIT_FORMAT_RGBA_8888 &&
            copybit_color_space != COPYBIT_FORMAT_RGB_565 &&
            copybit_color_space != COPYBIT_FORMAT_BGRA_8888 &&
            copybit_color_space != COPYBIT_FORMAT_RGBA_5551 &&
            copybit_color_space != COPYBIT_FORMAT_RGBA_4444
      )
        return -1;

    if (full_width == width && full_height == height) {
        unsigned int full_frame_size  = 0;

        if (copybit_color_space == COPYBIT_FORMAT_RGBA_8888)
            full_frame_size = ((full_width * full_height) << 2);
        else // RGB16
            full_frame_size = ((full_width * full_height) << 1);

        memcpy(dst_virt_addr, src_virt_addr, full_frame_size);
    } else {
        if (copybit_color_space == COPYBIT_FORMAT_RGBA_8888) {
            // 32bit
            real_full_width  = full_width  << 2;
            real_full_height = full_height << 2;
            real_start_x     = start_x     << 2;
            real_width       = width       << 2;
        } else {
            // 16bit
            real_full_width  = full_width  << 1;
            real_full_height = full_height << 1;
            real_start_x     = start_x     << 1;
            real_width       = width       << 1;
        }

        current_src_addr += (real_full_width * start_y);
        current_dst_addr += (real_full_width * start_y);

        if (full_width == width) {
            memcpy(current_dst_addr, current_src_addr, real_width * height);
        } else {
            for(y = 0; y < height; y++) {
                memcpy(current_dst_addr + real_start_x,
                        current_src_addr + real_start_x,
                        real_width);

                current_src_addr += real_full_width;
                current_dst_addr += real_full_width;
            }
        }
    }

    return 0;
}
