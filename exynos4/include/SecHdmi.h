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
** 
** @author Sangwoo, Park(sw5771.park@samsung.com)
** @date   2010-09-10
** 
*/


#ifndef __SEC_HDMI_H__
#define __SEC_HDMI_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include "s5p_tvout.h"

#include "sec_g2d.h"
#include "s3c_lcd.h"
#include "SecFimc.h"
#include "../libsForhdmi/libedid/libedid.h"
#include "../libsForhdmi/libcec/libcec.h"
//#include "libhpd.h"

#if defined(BOARD_USES_FIMGAPI)
#include "FimgApi.h"
#endif

#include <linux/fb.h>

#include <hardware/copybit.h>
#include <hardware/hardware.h>

#include <utils/threads.h>


namespace android {


#define TVOUT_DEV       "/dev/video14"
#define DEFAULT_FB      0
#define TVOUT_FB_G0     10
#define TVOUT_FB_G1     11

#define HDMI_FIMC_OUTPUT_BUF_NUM 3

#define ALIGN(x, a)    (((x) + (a) - 1) & ~((a) - 1))

#if defined(STD_NTSC_M)
    #define DEFAULT_OUPUT_MODE            (COMPOSITE_OUTPUT_MODE)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (1080960) // 1080P_60
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_1080P_60)
    #define DEFAULT_COMPOSITE_STD         (COMPOSITE_STD_NTSC_M)
#elif (STD_1080P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (1080960) // 1080P_60
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_1080P_60)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#elif defined(STD_720P) 
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (720960) // 720P_60
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_720P_60)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#elif defined(STD_480P) 
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (4809601) // 480P_60_4_3
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_480P_60_16_9)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#else
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (4809602) // 480P_60_4_3
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_480P_60_4_3)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#endif

// this is for testing... (very basic setting.)
//#define DEFAULT_HDMI_RESOLUTION_VALUE (4809602) // 480P_60_4_3
//#define DEFAULT_HDMI_STD_ID           (V4L2_STD_480P_60_4_3)

enum hdp_cable_status{
    HPD_CABLE_OUT = 0, // HPD_CABLE_OUT indicates HDMI cable out.
    HPD_CABLE_IN       // HPD_CABLE_IN indicates HDMI cable in.
};

enum state {
	OFF = 0,
	ON = 1,
	NOT_SUPPORT = 2,
};

enum tv_mode{
    HDMI_OUTPUT_MODE_YCBCR = 0,
    HDMI_OUTPUT_MODE_RGB = 1,
    HDMI_OUTPUT_MODE_DVI = 2,
    COMPOSITE_OUTPUT_MODE = 3
};

enum composite_std{
    COMPOSITE_STD_NTSC_M = 0,
    COMPOSITE_STD_PAL_BDGHI = 1,
    COMPOSITE_STD_PAL_M = 2,
    COMPOSITE_STD_PAL_N = 3,
    COMPOSITE_STD_PAL_Nc = 4,
    COMPOSITE_STD_PAL_60 = 5,
    COMPOSITE_STD_NTSC_443 = 6
};

enum hdmi_layer
{
    HDMI_LAYER_BASE   = 0,
    HDMI_LAYER_VIDEO,
    HDMI_LAYER_GRAPHIC_0,
    HDMI_LAYER_GRAPHIC_1,
    HDMI_LAYER_MAX,
};

int tvout_init(v4l2_std_id std_id);
int tvout_v4l2_querycap(int fp);
int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id);
int tvout_v4l2_s_std(int fp, v4l2_std_id std_id);
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id);
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output);
int tvout_v4l2_s_output(int fp, int index);
int tvout_v4l2_g_output(int fp, int *index);
int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr);
int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr);
int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame);
int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame);
int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect);
int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect);
int tvout_v4l2_start_overlay(int fp);
int tvout_v4l2_stop_overlay(int fp);


int hdmi_set_v_param(int layer,
        int src_w, int src_h, int colorFormat,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_w, int dst_h);
int hdmi_gl_set_param(int layer,
                      int src_w, int src_h,
                      int dst_x, int dst_y, int dst_w, int dst_h,
                      unsigned int ui_base_address, unsigned int ui_top_c_address);
int hdmi_cable_status();
int hdmi_cable_status();
int hdmi_outputmode_2_v4l2_output_type(int output_mode);
int hdmi_v4l2_output_type_2_outputmode(int output_mode);
static int composite_std_2_v4l2_std_id(int std);
static int hdmi_check_output_mode();
static int hdmi_check_resolution(v4l2_std_id std_id);
int hdmi_resolution_2_std_id(unsigned int resolution, int * w, int * h, v4l2_std_id * std_id);
int hdmi_enable_hdcp(unsigned int hdcp_en);

static int hdmi_check_audio(void);

//#define DEBUG_HDMI_HW_LEVEL
#define BOARD_USES_EDID
//#define BOARD_USES_CEC

class SecHdmi: virtual public RefBase
{
public :
    enum HDMI_LAYER
    {
        HDMI_LAYER_BASE   = 0,
        HDMI_LAYER_VIDEO,
        HDMI_LAYER_GRAPHIC_0,
        HDMI_LAYER_GRAPHIC_1,
        HDMI_LAYER_MAX,
    };

private :
    class CECThread: public Thread
    {
        public:
            bool                mFlagRunning;

        private:
            sp<SecHdmi>         mSecHdmi;
            Mutex               mThreadLoopLock;
            Mutex               mThreadControlLock;
            virtual bool        threadLoop();
            enum CECDeviceType  mDevtype;
            int                 mLaddr;
            int                 mPaddr;

        public:
            CECThread(sp<SecHdmi> secHdmi)
                :Thread(false),
                mFlagRunning(false),
                mSecHdmi(secHdmi),
                mDevtype(CEC_DEVICE_PLAYER),
                mLaddr(0),
                mPaddr(0){
            };
            virtual ~CECThread();

            bool start();
            bool stop();

    };

    Mutex        mLock;

    sp<CECThread>               mCECThread;

    bool         mFlagCreate;
    bool         mFlagConnected;
    bool         mFlagLayerEnable[HDMI_LAYER_MAX];
    bool         mFlagHdmiStart[HDMI_LAYER_MAX];
    
    int          mSrcWidth[HDMI_LAYER_MAX];
    int          mSrcHeight[HDMI_LAYER_MAX];
    int          mSrcColorFormat[HDMI_LAYER_MAX];
    int          mHdmiSrcWidth[HDMI_LAYER_MAX];
    int          mHdmiSrcHeight[HDMI_LAYER_MAX];

    int          mHdmiDstWidth;
    int          mHdmiDstHeight;
    unsigned int mHdmiSrcYAddr;
    unsigned int mHdmiSrcCbCrAddr;

    int          mHdmiOutputMode;
    unsigned int mHdmiResolutionValue;
    v4l2_std_id  mHdmiStdId;
    unsigned int mCompositeStd;
    bool         mHdcpMode;
    int          mAudioMode;
    unsigned int mUIRotVal;

    int          mCurrentHdmiOutputMode;
    unsigned int mCurrentHdmiResolutionValue;
    bool         mCurrentHdcpMode;
    int          mCurrentAudioMode;
    bool         mHdmiInfoChange;

    bool         mFlagFimcStart;
    int          mFimcDstColorFormat;
    unsigned int mFimcReservedMem[HDMI_FIMC_OUTPUT_BUF_NUM];
    unsigned int mFimcCurrentOutBufIndex;
    SecFimc      mSecFimc;

    unsigned int mHdmiResolutionValueList[11];
    int          mHdmiSizeOfResolutionValueList;

    int          mDefaultFBFd;
public :

    SecHdmi();
    virtual ~SecHdmi();
    bool        create(void);
    bool        destroy(void);
    inline bool flagCreate(void) { return mFlagCreate; }

    bool        connect(void);
    bool        disconnect(void);

    bool        flagConnected(void);

    bool        flush(int srcW, int srcH, int srcColorFormat,
                      unsigned int srcPhysYAddr, unsigned int srcPhysCbAddr,
                      int dstX, int dstY,
                      int hdmiLayer);

    bool        clear(int hdmiLayer);
    
    bool        enableGraphicLayer(int hdmiLayer);
    bool        disableGraphicLayer(int hdmiLayer);
    bool        startHdmi(int hdmiLayer);
    bool        stopHdmi(int hdmiLayer);

    bool        setHdmiOutputMode(int hdmiOutputMode, bool forceRun = false);
    bool        setHdmiResolution(unsigned int hdmiResolutionValue, bool forceRun = true);
    bool        setHdcpMode(bool hdcpMode, bool forceRun = false);
    bool        setUIRotation(unsigned int rotVal);

private:

    bool        m_reset(int w, int h, int colorFormat, int hdmiLayer);
    bool        m_startHdmi(int hdmiLayer);
    bool        m_stopHdmi(int hdmiLayer);

    bool        m_setHdmiOutputMode(int hdmiOutputMode);
    bool        m_setHdmiResolution(unsigned int hdmiResolutionValue);
    bool        m_setCompositeResolution(unsigned int compositeStdId);
    bool        m_setHdcpMode(bool hdcpMode);
    bool        m_setAudioMode(int audioMode);


    int         m_resolutionValueIndex(unsigned int ResolutionValue);

    bool        m_flagHWConnected(void);    

};

}; // namespace android

#endif //__SEC_HDMI_H__


