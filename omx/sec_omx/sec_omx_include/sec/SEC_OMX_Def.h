/*
 *
 * Copyright 2010 Samsung Electronics S.LSI Co. LTD
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
 * @file    SEC_OMX_Def.h
 * @brief   SEC_OMX specific define
 * @author  SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version    1.0.2
 * @history
 *   2010.7.15 : Create
 */

#ifndef SEC_OMX_DEF
#define SEC_OMX_DEF

#include "OMX_Types.h"
#include "OMX_IVCommon.h"

#define VERSIONMAJOR_NUMBER                1
#define VERSIONMINOR_NUMBER                0
#define REVISION_NUMBER                    0
#define STEP_NUMBER                        0


#define MAX_OMX_COMPONENT_NUM              20
#define MAX_OMX_COMPONENT_ROLE_NUM         10
#define MAX_OMX_COMPONENT_NAME_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_ROLE_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_LIBNAME_SIZE     OMX_MAX_STRINGNAME_SIZE * 2
#define MAX_OMX_MIMETYPE_SIZE              OMX_MAX_STRINGNAME_SIZE

#define MAX_TIMESTAMP        17
#define MAX_FLAGS            17


typedef enum _SEC_CODEC_TYPE
{
    SW_CODEC,
    HW_VIDEO_DEC_CODEC,
    HW_VIDEO_ENC_CODEC,
    HW_AUDIO_CODEC
} SEC_CODEC_TYPE;

typedef struct _SEC_OMX_PRIORITYMGMTTYPE
{
    OMX_U32 nGroupPriority; /* the value 0 represents the highest priority */
                            /* for a group of components                   */
    OMX_U32 nGroupID;
} SEC_OMX_PRIORITYMGMTTYPE;

typedef enum _SEC_OMX_INDEXTYPE
{
    OMX_IndexVendorThumbnailMode        = 0x7F000001,
    OMX_IndexConfigVideoIntraPeriod     = 0x7F000002,
    OMX_COMPONENT_CAPABILITY_TYPE_INDEX = 0xFF7A347 /*for Android*/
} SEC_OMX_INDEXTYPE;

typedef enum _SEC_OMX_ERRORTYPE
{
    OMX_ErrorNoEOF = 0x90000001,
    OMX_ErrorInputDataDecodeYet,
    OMX_ErrorInputDataEncodeYet,
    OMX_ErrorMFCInit
} SEC_OMX_ERRORTYPE;

typedef enum _SEC_OMX_COMMANDTYPE
{
    SEC_OMX_CommandComponentDeInit = 0x7F000001,
    SEC_OMX_CommandEmptyBuffer,
    SEC_OMX_CommandFillBuffer
} SEC_OMX_COMMANDTYPE;

typedef enum _SEC_OMX_TRANS_STATETYPE {
    SEC_OMX_TransStateInvalid,
    SEC_OMX_TransStateLoadedToIdle,
    SEC_OMX_TransStateIdleToExecuting,
    SEC_OMX_TransStateExecutingToIdle,
    SEC_OMX_TransStateIdleToLoaded,
    SEC_OMX_TransStateMax = 0X7FFFFFFF
} SEC_OMX_TRANS_STATETYPE;

typedef enum _SEC_OMX_COLOR_FORMATTYPE {
#ifndef USE_SAMSUNG_COLORFORMAT
    OMX_SEC_COLOR_FormatNV12TPhysicalAddress = OMX_COLOR_FormatYUV420Planar,
    OMX_SEC_COLOR_FormatNV12LPhysicalAddress = OMX_COLOR_FormatYUV420Planar
#else
    OMX_SEC_COLOR_FormatNV12TPhysicalAddress = 0x7F000001, /**< Reserved region for introducing Vendor Extensions */
    OMX_SEC_COLOR_FormatNV12LPhysicalAddress = 0x7F000002,
    OMX_SEC_COLOR_FormatNV12Tiled            = 0x7FC00002  /* 0x7FC00002 */
#endif
}SEC_OMX_COLOR_FORMATTYPE;

typedef enum _SEC_OMX_SUPPORTFORMAT_TYPE
{
    supportFormat_1 = 0x00,
    supportFormat_2,
    supportFormat_3,
    supportFormat_4,
    supportFormat_5
} SEC_OMX_SUPPORTFORMAT_TYPE;

typedef enum _SEC_OMX_FRAME_TYPE
{
    SEC_OMX_UnknownType = 0x00,
    SEC_OMX_IFrameType,
    SEC_OMX_PFrameType,
    SEC_OMX_BFrameType
} SEC_OMX_FRAME_TYPE;

/* for Android */
typedef struct _OMXComponentCapabilityFlagsType
{
    /* OMX COMPONENT CAPABILITY RELATED MEMBERS */
    OMX_BOOL iIsOMXComponentMultiThreaded;
    OMX_BOOL iOMXComponentSupportsExternalOutputBufferAlloc;
    OMX_BOOL iOMXComponentSupportsExternalInputBufferAlloc;
    OMX_BOOL iOMXComponentSupportsMovableInputBuffers;
    OMX_BOOL iOMXComponentSupportsPartialFrames;
    OMX_BOOL iOMXComponentUsesNALStartCodes;
    OMX_BOOL iOMXComponentCanHandleIncompleteFrames;
    OMX_BOOL iOMXComponentUsesFullAVCFrames;
} OMXComponentCapabilityFlagsType;

typedef struct _SEC_OMX_VIDEO_PROFILELEVEL
{
    OMX_S32  profile;
    OMX_S32  level;
} SEC_OMX_VIDEO_PROFILELEVEL;


#ifndef __OMX_EXPORTS
#define __OMX_EXPORTS
#define SEC_EXPORT_REF __attribute__((visibility("default")))
#define SEC_IMPORT_REF __attribute__((visibility("default")))
#endif

#endif
