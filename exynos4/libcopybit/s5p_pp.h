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

#ifndef _S5P_PP_H_
#define _S5P_PP_H_

#define HW_PMEM_USE

#define NUM_OF_MEMORY_OBJECT     2
#define MAX_RESIZING_RATIO_LIMIT 63

#include <linux/videodev2.h>
#include "SecFimc.h"

#ifdef HW_G2D_USE
#ifdef USE_FIMGAPI
#include "FimgApi.h"
#endif

#include "sec_g2d.h"
#endif

#include "s3c_mem.h"

//------------ STRUCT ---------------------------------//

typedef struct _s5p_rect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
}s5p_rect;

typedef struct _s5p_img {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t offset;
    uint32_t base;
    int memory_id;
}s5p_img;

#ifdef HW_PMEM_USE
typedef struct _sec_pmem_alloc{
    int          fd;
    int          src_size;
    int          dst_size;
    unsigned int src_virt_addr;
    unsigned int src_phys_addr;
    unsigned int dst_virt_addr;
    unsigned int dst_phys_addr;
} sec_pmem_alloc_t;

typedef struct _sec_pmem_t{
    int              pmem_master_fd;
    void             *pmem_master_base;
    int              pmem_total_size;
    sec_pmem_alloc_t sec_pmem_alloc;
} sec_pmem_t;
#endif

typedef struct _s5p_pp_t {
    SecFimc sec_fimc;
#ifdef HW_G2D_USE
    sec_g2d_t s5p_g2d;
#endif
#ifdef HW_PMEM_USE
    sec_pmem_t sec_pmem;
#endif
    s3c_mem_t s3c_mem[NUM_OF_MEMORY_OBJECT];
}s5p_pp_t;

typedef struct _s5p_pp_addr_t {
    unsigned int addr_y;
    unsigned int addr_cb;
    unsigned int addr_cr;
}s5p_pp_addr_t;

// HW IP path for post processing 
enum {
    PATH_FIMC,          // src img -> FIMC -> dst img (FB or mem)
    PATH_G2D_FIMC,      // src img -> FIMC -> internal output dma -> G2D
                        // -> dst img (FB or mem)
    PATH_G2D,           // src img -> G2D -> dst img
    PATH_NOT_SUPPORTED  // can not support
};

//---------------------- Function Declarations -----------------------//
int createPP(void **s5p_pp);
int destroyPP(void **s5p_pp);
int doPP(void *s5p_pp, int copybit_rotate_flag, int copybit_alpha_flag,
        s5p_img *src_img, s5p_rect *src_rect, s5p_img *dst_img,
        s5p_rect *dst_rect);
#endif
