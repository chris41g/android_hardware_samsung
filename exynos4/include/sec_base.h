/* Copyright (c) 2011 Samsung Electronics Co, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 *

 * Alternatively, Licensed under the Apache License, Version 2.0 (the "License");
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



#ifndef __SAMSUNG_SYSLSI_SEC_BASE_H__
#define __SAMSUNG_SYSLSI_SEC_BASE_H__

//---------------------------------------------------------//
// Include
//---------------------------------------------------------//

#include <utils/Log.h>
#include <hardware/hardware.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/videodev2.h>
#include "videodev2_samsung.h"

#ifdef __cplusplus
}
#endif

//---------------------------------------------------------//
// Common structure                                        //
//---------------------------------------------------------//
struct ADDRS {
    unsigned int addr_y;
    unsigned int addr_cbcr;
    unsigned int buf_idx;
    unsigned int reserved;
};

#define ALIGN_TO_32B(x)   ((((x) + (1 <<  5) - 1) >>  5) <<  5)
#define ALIGN_TO_128B(x)  ((((x) + (1 <<  7) - 1) >>  7) <<  7)
#define ALIGN_TO_8KB(x)   ((((x) + (1 << 13) - 1) >> 13) << 13)

#define GET_32BPP_FRAME_SIZE(w, h)  (((w) * (h)) << 2)
#define GET_24BPP_FRAME_SIZE(w, h)  (((w) * (h)) * 3)
#define GET_16BPP_FRAME_SIZE(w, h)  (((w) * (h)) << 1)

#endif //__SAMSUNG_SYSLSI_SEC_BASE_H__
