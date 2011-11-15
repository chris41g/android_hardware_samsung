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

//#define LOG_NDEBUG 0
#define LOG_TAG "CameraHardwareSec"
#include <utils/Log.h>

#include "SecCameraHWInterface.h"
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>

#if defined(BOARD_USES_CAMERA_OVERLAY)
#include <hardware/overlay.h>
#include <ui/Overlay.h>
#define CACHEABLE_BUFFERS       0x1
#define ALL_BUFFERS_FLUSHED     -66
#endif

#if 0 //defined(BOARD_USES_HDMI)
#include <surfaceflinger/ISurface.h>
#endif

namespace android {

struct ADDRS {
    unsigned int addr_y;
    unsigned int addr_cbcr;
#if defined(BOARD_USES_CAMERA_OVERLAY)
    int buf_idx;
#endif
};

struct ADDRS_CAP {
    unsigned int addr_y;
    unsigned int width;
    unsigned int height;
};

CameraHardwareSec::CameraHardwareSec(int cameraId)
                  : mParameters(),
                    mPreviewHeap(0),
                    mRawHeap(0),
                    mRecordHeap(0),
                    mJpegHeap(0),
                    mSecCamera(NULL),
                    mPreviewRunning(false),
                    mRecordRunning(false),
                    mCaptureInProgress(false),
                    mPreviewFrameRateMicrosec(33333),
#if defined(BOARD_USES_CAMERA_OVERLAY)
                    mUseOverlay(false),
                    mOverlayBufferIdx(0),
#endif
#if 0 //defined(BOARD_USES_HDMI)
                    mISurface(0),
#endif
                    mNotifyCb(0),
                    mDataCb(0),
                    mDataCbTimestamp(0),
                    mCallbackCookie(0),
                    mMsgEnabled(0),
                    mAFMode(SecCamera::AF_MODE_AUTO)
{
    LOGV("%s :", __FUNCTION__);

    mSecCamera = SecCamera::createInstance();
    LOGE_IF(mSecCamera == NULL, "ERR(%s):Fail on mSecCamera object creation", __FUNCTION__);

    if(mSecCamera->flagCreate() == 0 &&
            mSecCamera->Create(cameraId) < 0) {
        LOGE("ERR(%s):Fail on mSecCamera->Create", __FUNCTION__);
    }


#ifndef PREVIEW_USING_MMAP
    int previewHeapSize = sizeof(struct ADDRS) * kBufferCount;
    LOGV("mPreviewHeap : MemoryHeapBase(previewHeapSize(%d))", previewHeapSize);
    mPreviewHeap = new MemoryHeapBase(previewHeapSize);
    if (mPreviewHeap->getHeapID() < 0) {
        LOGE("ERR(%s): Preview heap creation fail", __FUNCTION__);
        mPreviewHeap.clear();
    }
#endif

    int recordHeapSize = sizeof(struct ADDRS) * kBufferCount;
    LOGV("mRecordHeap : MemoryHeapBase(recordHeapSize(%d))", recordHeapSize);

    mRecordHeap = new MemoryHeapBase(recordHeapSize);
    if (mRecordHeap->getHeapID() < 0) {
        LOGE("ERR(%s): Record heap creation fail", __FUNCTION__);
        mRecordHeap.clear();
    }

    int rawHeapSize = sizeof(struct ADDRS_CAP);
    LOGV("mRawAddrHeap : MemoryHeapBase(rawAddrHeapSize(%d))", rawHeapSize);
    mRawAddrHeap = new MemoryHeapBase(rawHeapSize);
    if (mRawAddrHeap->getHeapID() < 0) {
        LOGE("ERR(%s): Raw heap creation fail", __FUNCTION__);
        mRawAddrHeap.clear();
    }

    memset(&mFrameRateTimer,  0, sizeof(DurationTimer));
    memset(&mGpsInfo, 0, sizeof(gps_info));

    m_initDefaultParameters(cameraId);

    mPreviewThread = new PreviewThread(this);
}

void CameraHardwareSec::m_initDefaultParameters(int cameraId)
{
    LOGV("++%s :", __FUNCTION__);

    if(mSecCamera == NULL) {
        LOGE("ERR(%s):mSecCamera object is NULL", __FUNCTION__);
        return;
    }

    CameraParameters p;
    CameraParameters ip;

    int preview_max_width	= 0;
    int preview_max_height	= 0;
    int snapshot_max_width	= 0;
    int snapshot_max_height = 0;

    /* set camera ID & reset camera */
    mSecCamera->setCameraId(cameraId);
    if (cameraId == SecCamera::CAMERA_ID_BACK) {
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
              "1280x720,720x480,720x480,640x480,320x240,176x144");
        p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
              "3264x2448,3264x1968,2560x1920,2048x1536,2048x1536,2048x1232,1600x1200,1600x960,800x480,640x480");
    } else {
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, "640x480");
        p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "640x480");
    }

    // If these fail, then we are using an invalid cameraId and we'll leave the
    // sizes at zero to catch the error.
    if (mSecCamera->getPreviewMaxSize(&preview_max_width,
                                      &preview_max_height) < 0)
        LOGE("getPreviewMaxSize fail (%d / %d) \n",
             preview_max_width, preview_max_height);
    if (mSecCamera->getSnapshotMaxSize(&snapshot_max_width,
                                       &snapshot_max_height) < 0)
        LOGE("getSnapshotMaxSize fail (%d / %d) \n",
             snapshot_max_width, snapshot_max_height);


#ifdef PREVIEW_USING_MMAP
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);
#else
    p.setPreviewFormat("yuv420sp_ycrcb");
#endif
    p.setPreviewSize(preview_max_width, preview_max_height);

	p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    p.setPictureSize(snapshot_max_width, snapshot_max_height);

    p.set(CameraParameters::KEY_JPEG_QUALITY, "100"); // maximum quality

    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,
          CameraParameters::PIXEL_FORMAT_YUV420SP);
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
          CameraParameters::PIXEL_FORMAT_JPEG);
    p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,
          CameraParameters::PIXEL_FORMAT_YUV420SP);

    if (cameraId == SecCamera::CAMERA_ID_BACK) {
        p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
              "320x240,0x0");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "320");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "240");
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "30");
        p.setPreviewFrameRate(30);
    } else {
        p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
              "160x120,0x0");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "160");
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "120");
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "15");
        p.setPreviewFrameRate(15);
    }

    p.set("scene-mode-values",   "auto,beach,candlelight,fireworks,landscape,night,night-portrait,party,portrait,snow,sports,steadyphoto,sunset");
    p.set("scene-mode",          "auto");

    if (cameraId == SecCamera::CAMERA_ID_BACK) {
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(15000,30000)");
        p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "15000,30000");
    } else {
        p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(7500,15000)");
        p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "7500,15000");
    }

    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "100");
    p.set(CameraParameters::KEY_ROTATION, 0);
    p.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);

    mParameters = p;

    setParameters(p);

    mSecCamera->setContrast(CONTRAST_DEFAULT);
    mSecCamera->setSharpness(SHARPNESS_DEFAULT);
    mSecCamera->setSaturation(SATURATION_DEFAULT);

    if (cameraId == SecCamera::CAMERA_ID_BACK)
        mSecCamera->setFrameRate(30);
    else
        mSecCamera->setFrameRate(15);
    LOGV("--%s :", __FUNCTION__);

}

CameraHardwareSec::~CameraHardwareSec()
{
    LOGV("%s :", __FUNCTION__);

    this->release();

    mSecCamera = NULL;

    singleton.clear();
}

sp<IMemoryHeap> CameraHardwareSec::getPreviewHeap() const
{
    return mPreviewHeap;
}

sp<IMemoryHeap> CameraHardwareSec::getRawHeap() const
{
    return mRawAddrHeap;
}

void CameraHardwareSec::setCallbacks(notify_callback notify_cb,
                                      data_callback data_cb,
                                      data_callback_timestamp data_cb_timestamp,
                                      void* user)
{
    mNotifyCb        = notify_cb;
    mDataCb          = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mCallbackCookie  = user;
}

void CameraHardwareSec::enableMsgType(int32_t msgType)
{
    LOGV("%s : msgType = 0x%x, mMsgEnabled before = 0x%x",
         __FUNCTION__, msgType, mMsgEnabled);
    mMsgEnabled |= msgType;
    LOGV("%s : mMsgEnabled = 0x%x", __FUNCTION__, mMsgEnabled);
}

void CameraHardwareSec::disableMsgType(int32_t msgType)
{
    LOGV("%s : msgType = 0x%x, mMsgEnabled before = 0x%x",
         __FUNCTION__, msgType, mMsgEnabled);
    mMsgEnabled &= ~msgType;
    LOGV("%s : mMsgEnabled = 0x%x", __FUNCTION__, mMsgEnabled);
}

bool CameraHardwareSec::msgTypeEnabled(int32_t msgType)
{
    return (mMsgEnabled & msgType);
}

#if defined(BOARD_USES_CAMERA_OVERLAY)
bool CameraHardwareSec::useOverlay()
{
    LOGV("%s: returning true", __FUNCTION__);
    return true;
}

status_t CameraHardwareSec::setOverlay(const sp<Overlay> &overlay)
{
    LOGV("%s :", __FUNCTION__);

    int overlayWidth  = 0;
    int overlayHeight = 0;
    unsigned int overlayFrameSize = 0;

    if (overlay == NULL) {
        LOGV("%s : overlay == NULL", __FUNCTION__);
        goto setOverlayFail;
    }
    LOGV("%s : overlay = %p", __FUNCTION__, overlay->getHandleRef());

    if (overlay->getHandleRef()== NULL && mUseOverlay == true) {
        if (mOverlay != 0)
            mOverlay->destroy();

        mOverlay = NULL;
        mUseOverlay = false;

        return NO_ERROR;
    }

    if (overlay->getStatus() != NO_ERROR) {
        LOGE("ERR(%s):overlay->getStatus() fail", __FUNCTION__);
        goto setOverlayFail;
    }

    mSecCamera->getPreviewSize(&overlayWidth, &overlayHeight, &overlayFrameSize);

    if (overlay->setCrop(0, 0, overlayWidth, overlayHeight) != NO_ERROR) {
        LOGE("ERR(%s)::(mOverlay->setCrop(0, 0, %d, %d) fail", __FUNCTION__, overlayWidth, overlayHeight);
        goto setOverlayFail;
    }

    mOverlay = overlay;
    mUseOverlay = true;

    return NO_ERROR;

setOverlayFail :
    if (mOverlay != 0)
        mOverlay->destroy();
    mOverlay = 0;

    mUseOverlay = false;

    return UNKNOWN_ERROR;
}
#endif

#if 0 //defined(BOARD_USES_HDMI)
status_t CameraHardwareSec::setSurface(const sp<ISurface> &surface)
{
    mISurface = surface;
    return NO_ERROR;
}
#endif

// ---------------------------------------------------------------------------

int CameraHardwareSec::previewThread()
{
    sp<MemoryBase> previewBuffer;
    sp<MemoryBase> recordBuffer;

    {
        LOGV(" m_previewThreadFunc ");

        int index;
        unsigned int phyYAddr = 0;
        unsigned int phyCAddr = 0;

        index = mSecCamera->getPreview();
        if(index < 0) {
            LOGE("ERR(%s):Fail on SecCamera->getPreview()", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

		nsecs_t timestamp = systemTime(SYSTEM_TIME_MONOTONIC);

#ifdef BOARD_SUPPORT_SYSMMU
        phyYAddr = mSecCamera->getSecureID(index);
        phyCAddr = mSecCamera->getOffset(index);
#else
        phyYAddr = mSecCamera->getPhyAddrY(index);
        phyCAddr = mSecCamera->getPhyAddrC(index);
#endif
        if(phyYAddr == 0xffffffff || phyCAddr == 0xffffffff) {
            LOGE("ERR(%s):Fail on SecCamera getPhyAddr Y addr = %0x C addr = %0x", __FUNCTION__, phyYAddr, phyCAddr);
            return UNKNOWN_ERROR;
        }

        int width, height, offset;
        unsigned int frame_size;
        mSecCamera->getPreviewSize(&width, &height, &frame_size);

#ifdef PREVIEW_USING_MMAP
        {
            //offset = (frame_size + 16) * index;
            offset = (frame_size) * index;
            previewBuffer = new MemoryBase(mPreviewHeap, offset, frame_size);

            //memcpy(static_cast<unsigned char *>(mPreviewHeap->base()) + (offset + frame_size	), &phyYAddr, 4);
            //memcpy(static_cast<unsigned char *>(mPreviewHeap->base()) + (offset + frame_size + 4), &phyCAddr, 4);

	#if defined(BOARD_USES_CAMERA_OVERLAY)
            if (mUseOverlay) {
                int ret;
                struct ADDRS addr;
                mOverlayBufferIdx ^= 1;
            #if 1
                addr.addr_y = phyYAddr;
                addr.addr_cbcr = phyCAddr;
                addr.buf_idx = mOverlayBufferIdx;

                ret = mOverlay->queueBuffer((void*)&addr);
            #else
                memcpy(static_cast<unsigned char*>(mPreviewHeap->base()) + sizeof(phyYAddr) + sizeof(phyCAddr),
                        &mOverlayBufferIdx, sizeof(mOverlayBufferIdx));

                ret = mOverlay->queueBuffer((void*)(static_cast<unsigned char *>(mPreviewHeap->base())));
            #endif
                if (ret == ALL_BUFFERS_FLUSHED) {
                } else if (ret == -1) {
                    LOGE("ERR(%s):overlay queueBuffer fail", __FUNCTION__);
                } else {
                    overlay_buffer_t overlay_buffer;
                    ret = mOverlay->dequeueBuffer(&overlay_buffer);
                }
            }
	#endif
        }
#else
        {
            struct ADDRS *addrs = (struct ADDRS *)mPreviewHeap->base();

            previewBuffer = new MemoryBase(mPreviewHeap, index * sizeof(struct ADDRS), sizeof(struct ADDRS));;
            addrs[index].addr_y    = phyYAddr;
            addrs[index].addr_cbcr = phyCAddr;
	#if defined(BOARD_USES_CAMERA_OVERLAY)
            if (mUseOverlay) {
                int ret;
                mOverlayBufferIdx ^= 1;
                addrs[index].buf_idx = mOverlayBufferIdx;

                ret = mOverlay->queueBuffer((void*)(static_cast<unsigned char *>(mPreviewHeap->base() + index * sizeof(struct ADDRS))));

                if (ret == ALL_BUFFERS_FLUSHED) {
                } else if (ret == -1) {
                    LOGE("ERR(%s):overlay queueBuffer fail", __FUNCTION__);
                } else {
                    overlay_buffer_t overlay_buffer;
                    ret = mOverlay->dequeueBuffer(&overlay_buffer);
                }
            }
	#endif
        }
#endif //PREVIEW_USING_MMAP

#if 0 //defined(BOARD_USES_HDMI)
        if(mISurface != NULL)
            mISurface->postBuffer2HDMI(phyYAddr, phyCAddr, width, height);
#endif

        mStateLock.lock();

        if(mRecordRunning == true) {
            LOGV("mRecordRunning++");
#ifdef DUAL_PORT_RECORDING
            index = mSecCamera->getRecord();
            if(index < 0) {
                LOGE("ERR(%s):Fail on SecCamera->getRecord()", __FUNCTION__);
                return UNKNOWN_ERROR;
            }
            phyYAddr = mSecCamera->getRecPhyAddrY(index);
            phyCAddr = mSecCamera->getRecPhyAddrC(index);
            if(phyYAddr == 0xffffffff || phyCAddr == 0xffffffff) {
                LOGE("ERR(%s):Fail on SecCamera getRectPhyAddr Y addr = %0x C addr = %0x",
                        __FUNCTION__, phyYAddr, phyCAddr);
                return UNKNOWN_ERROR;
            }
#endif//DUAL_PORT_RECORDING

            struct ADDRS *addrs = (struct ADDRS *)mRecordHeap->base();

            recordBuffer = new MemoryBase(mRecordHeap, index * sizeof(struct ADDRS), sizeof(struct ADDRS));
            addrs[index].addr_y    = phyYAddr;
            addrs[index].addr_cbcr = phyCAddr;
            // Notify the client of a new frame.
            if(mDataCbTimestamp && (mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)) {
                //nsecs_t timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
                LOGV("recording time = %lld us ", timestamp);
                mDataCbTimestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, recordBuffer, mCallbackCookie);
            }
            LOGV("mRecordRunning--");

        }
        mStateLock.unlock();
    }

    // schedule for main camera process
    usleep(1);
    if(previewBuffer != 0) {
        // Notify the client of a new frame. //Kamat --eclair
        if(mDataCb && (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME)) {
            mDataCb(CAMERA_MSG_PREVIEW_FRAME, previewBuffer, mCallbackCookie);
        }
    }

#if 0
    // Wait for it...
    if (mTimeStart.tv_sec == 0 && mTimeStart.tv_usec == 0) {
        gettimeofday(&mTimeStart, NULL);
    } else {
        gettimeofday(&mTimeStop, NULL);
        long time = measure_time(&mTimeStart, &mTimeStop);
        int delay = (mPreviewFrameRateMicrosec > time) ? mPreviewFrameRateMicrosec - time : 0;

        usleep(delay);
        //LOG_CAMERA_PREVIEW("delay = %d time = %ld us\n ", delay, time);
        gettimeofday(&mTimeStart, NULL);
    }
#endif
    return NO_ERROR;
}

status_t CameraHardwareSec::startPreview()
{
    int ret = 0;        //s1 [Apply factory standard]

    LOGV("++%s \n", __FUNCTION__);

    Mutex::Autolock lock(mStateLock);
    if (mPreviewRunning) {
        // already running
        LOGE("%s : preview thread already running", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (mCaptureInProgress) {
        LOGE("%s : capture in progress, not allowed", __FUNCTION__);
        return INVALID_OPERATION;
    }

    memset(&mTimeStart, 0, sizeof(mTimeStart));
    memset(&mTimeStop, 0, sizeof(mTimeStop));

    //mSecCamera->stopPreview();
    ret  = mSecCamera->startPreview();
    LOGV("%s : mSecCamera->startPreview() returned %d", __FUNCTION__, ret);

    if(ret < 0) {
        LOGE("ERR(%s):Fail on mSecCamera->startPreview()", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    #ifdef PREVIEW_USING_MMAP
    {
        if(mPreviewHeap != NULL)
            mPreviewHeap.clear();

        int width, height;
        unsigned int frameSize;

        mSecCamera->getPreviewSize(&width, &height, &frameSize);

        //unsigned int previewHeapSize = (frameSize + 16) * kBufferCount;
        unsigned int previewHeapSize = (frameSize) * kBufferCount;

        LOGV("MemoryHeapBase(fd(%d), size(%d), %d)", (int)mSecCamera->getCameraFd(), (size_t)(previewHeapSize), (uint32_t)0);

        mPreviewHeap = new MemoryHeapBase((int)mSecCamera->getCameraFd(), (size_t)(previewHeapSize), (uint32_t)0);
		/*
        for(int index=0; index < kBufferCount; index++)
        {
            mBuffers[index] = new MemoryBase(mPreviewHeap, frameSize*index, frameSize);
        } */

    }
    #endif

    mPreviewRunning = true;

//    if(mPreviewRunning == 0)
	mPreviewThread->run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);

    LOGV("--%s \n", __FUNCTION__);
    return NO_ERROR;
}


void CameraHardwareSec::stopPreview()
{
    LOGV("++%s \n", __FUNCTION__);

    if (!previewEnabled())
        return;

    /* request that the preview thread exit. we can wait because we're
     * called by CameraServices with a lock but it has disabled all preview
     * related callbacks so previewThread should not invoke any callbacks.
     */
    mPreviewThread->requestExitAndWait();


    if(mSecCamera && mSecCamera->stopPreview() < 0)
        LOGE("ERR(%s):Fail on mSecCamera->stopPreview()", __FUNCTION__);

    mPreviewRunning = false;
    LOGV("--%s \n", __FUNCTION__);
}

bool CameraHardwareSec::previewEnabled()
{
    Mutex::Autolock lock(mStateLock);
    LOGV("%s : %d", __FUNCTION__, mPreviewRunning);
    return mPreviewRunning;
}

// ---------------------------------------------------------------------------

status_t CameraHardwareSec::startRecording()
{
    Mutex::Autolock lock(mStateLock);
    LOGV("%s :", __FUNCTION__);

#ifdef DUAL_PORT_RECORDING
    if(mSecCamera->startRecord() < 0) {
        LOGE("ERR(%s):Fail on mSecCamera->startRecord()", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
#endif
    mRecordRunning = true;
    LOGD("--%s :", __FUNCTION__);
    return NO_ERROR;
}

void CameraHardwareSec::stopRecording()
{
    Mutex::Autolock lock(mStateLock);
    LOGV("++%s :", __FUNCTION__);

#ifdef DUAL_PORT_RECORDING
    if(mSecCamera->stopRecord() < 0)
    {
        LOGE("ERR(%s):Fail on mSecCamera->stopRecord()", __FUNCTION__);
        mRecordRunning = false;
        return;
    }
#endif

    mRecordRunning = false;
    LOGD("--%s :", __FUNCTION__);
}

bool CameraHardwareSec::recordingEnabled()
{
    LOGV("%s :", __FUNCTION__);

    return mRecordRunning;
}

void CameraHardwareSec::releaseRecordingFrame(const sp<IMemory>& mem)
{
    LOG_CAMERA_PREVIEW("%s :", __FUNCTION__);

    ssize_t offset; size_t size;
//    sp<MemoryBase>	   mem1	= mem;
//    sp<MemoryHeapBase> heap = mem->getMemory(&offset, &size);
//    LOGD("@@@(%s):offset(%d), size()%d", __FUNCTION__,offset,size);

//    mem1.clear();
//    heap.clear();

    sp<IMemoryHeap> heap = mem->getMemory(&offset, &size);
	mSecCamera->releaseframe(int(offset/size));

}

int CameraHardwareSec::m_autoFocusThreadFunc()
{
    LOGV("++%s :", __FUNCTION__);

    Mutex::Autolock autoFocusLock(&mAutofocusLock);

    int  flagFocused    = 0;

    if(mAFMode == SecCamera::AF_MODE_AUTO
       || mAFMode == SecCamera::AF_MODE_MACRO) {
        if(mSecCamera->runAF(SecCamera::FLAG_ON, &flagFocused) < 0)
            LOGE("ERR(%s):Fail on mSecCamera->runAF()", __FUNCTION__);
    }
    else
        flagFocused = 1;

    if(mNotifyCb && (mMsgEnabled & CAMERA_MSG_FOCUS)) {
        bool flagAFSucced = (flagFocused == 1) ? true : false;

        mNotifyCb(CAMERA_MSG_FOCUS, flagAFSucced, 0, mCallbackCookie);
    }

    LOGV("--%s :", __FUNCTION__);

    return NO_ERROR;
}

status_t CameraHardwareSec::autoFocus()
{
    LOGV("++%s :", __FUNCTION__);

    if (createThread(m_beginAutoFocusThread, this) == false)
        return UNKNOWN_ERROR;

    LOGV("--%s :", __FUNCTION__);
    return NO_ERROR;
}

status_t CameraHardwareSec::cancelAutoFocus()
{
    LOGV("++%s :", __FUNCTION__);

    int flagFocused = 0;
    if(mSecCamera->runAF(SecCamera::FLAG_OFF, &flagFocused) < 0)
        LOGE("ERR(%s):Fail on mSecCamera->runAF(FLAG_OFF)", __FUNCTION__);

    LOGV("--%s :", __FUNCTION__);

    return NO_ERROR;
}

int CameraHardwareSec::pictureThread()
{
    LOGV("++%s :", __FUNCTION__);

    int ret = NO_ERROR;

    sp<MemoryBase>  rawBuffer = NULL;
    sp<MemoryBase>  rawAddrBuffer = NULL;
    int             pictureWidth  = 0;
    int             pictureHeight = 0;
    int             pictureFormat = 0;
    int             pictureSize = 0;
    unsigned int    picturePhyAddr = 0;
    bool            flagShutterCallback = false;

    unsigned char * jpegData = NULL;
    int             jpegSize = 0;

    mSecCamera->getSnapshotSize(&pictureWidth, &pictureHeight, &pictureSize);

    LOGV("pictureWidth %d, pictureHeight %d, pictureSize %d", pictureWidth, pictureHeight, pictureSize);

    if((mMsgEnabled & CAMERA_MSG_RAW_IMAGE) && mDataCb) {
        pictureFormat = mSecCamera->getSnapshotPixelFormat();

        if(0 <= m_getGpsInfo(&mParameters, &mGpsInfo)) {
            if(mSecCamera->setGpsInfo(mGpsInfo.latitude,  mGpsInfo.longitude,
                        mGpsInfo.timestamp, mGpsInfo.altitude) < 0) {
                LOGE("%s::setGpsInfo fail.. but making jpeg is progressing\n", __FUNCTION__);
            }
        }

        rawBuffer = new MemoryBase(mRawHeap, 0, pictureSize);
        picturePhyAddr = mSecCamera->getSnapshot((unsigned char*)mRawHeap->base(), pictureSize);
        if(picturePhyAddr == 0) {
            LOGE("ERR(%s):Fail on SecCamera->getSnapshot()", __FUNCTION__);
            mStateLock.lock();
            mCaptureInProgress = false;
            mStateLock.unlock();
            ret = UNKNOWN_ERROR;
        }

        mStateLock.lock();
        mCaptureInProgress = false;
        mStateLock.unlock();

#if 0
        if(picturePhyAddr != 0) {
            rawAddrBuffer = new MemoryBase(mRawAddrHeap, 0, sizeof(struct ADDRS_CAP));
            struct ADDRS_CAP *addrs = (struct ADDRS_CAP *)mRawAddrHeap->base();

            addrs[0].addr_y = picturePhyAddr;
            addrs[0].width  = pictureWidth;
            addrs[0].height = pictureHeight;
            LOGV("addrs[0].addr_y 0x%x addrs[0].width %d addrs[0].height %d",
                    addrs[0].addr_y, addrs[0].width, addrs[0].height);
        }

        if(mNotifyCb && (mMsgEnabled & CAMERA_MSG_SHUTTER)) {
            image_rect_type size;
            size.width  = pictureWidth;
            size.height = pictureHeight;

            mNotifyCb(CAMERA_MSG_SHUTTER, (int32_t)&size, 0, mCallbackCookie);

            flagShutterCallback = true;
        }

        mDataCb(CAMERA_MSG_RAW_IMAGE, rawAddrBuffer, mCallbackCookie);

#ifdef FAST_PREVIEW_AFTER_PICTURE
        if(mSecCamera->flagPreviewStart() == SecCamera::FLAG_OFF) {
            if(mSecCamera->startPreview() < 0)
                LOGE("ERR(%s):Fail on mSecCamera->startPreview()", __FUNCTION__);
        }
#endif // FAST_PREVIEW_AFTER_PICTURE
#endif
    }

#if 0
    if(mNotifyCb
            && flagShutterCallback == false
            && (mMsgEnabled & CAMERA_MSG_SHUTTER)) {
        image_rect_type size;

        size.width  = pictureWidth;
        size.height = pictureHeight;

        mNotifyCb(CAMERA_MSG_SHUTTER, (int32_t)&size, 0, mCallbackCookie);

        flagShutterCallback = true;
    }
#endif

    if(mDataCb && (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)) {
        if(picturePhyAddr != 0)
            jpegData = mSecCamera->yuv2Jpeg((unsigned char*)mRawHeap->base(), 0, &jpegSize,
                    pictureWidth, pictureHeight, pictureFormat);

        sp<MemoryBase> jpegMem = NULL;
        if(jpegData != NULL) {
            sp<MemoryHeapBase> jpegHeap = new MemoryHeapBase(jpegSize);
            jpegMem  = new MemoryBase(jpegHeap, 0, jpegSize);
            memcpy(jpegHeap->base(), jpegData, jpegSize);
        }

        mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegMem, mCallbackCookie);

        mSecCamera->CloseJpegBuffer();
    }

    LOGV("--%s :", __FUNCTION__);

    return ret;
}

status_t CameraHardwareSec::takePicture()
{
    LOGV("++%s :", __FUNCTION__);

    this->stopPreview();

    if (mCaptureInProgress) {
        LOGE("%s : capture already in progress", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (createThread(m_beginPictureThread, this) == false)
        return UNKNOWN_ERROR;

    mStateLock.lock();
    mCaptureInProgress = true;
    mStateLock.unlock();

    return NO_ERROR;
}

status_t CameraHardwareSec::cancelPicture()
{
    return NO_ERROR;
}

status_t CameraHardwareSec::setParameters(const CameraParameters& params)
{
    LOGV("++%s :", __FUNCTION__);

    status_t ret = NO_ERROR;

    /* if someone calls us while picture thread is running, it could screw
     * up the sensor quite a bit so return error.  we can't wait because
     * that would cause deadlock with the callbacks
     */
    mStateLock.lock();
    if (mCaptureInProgress) {
        mStateLock.unlock();
        LOGE("%s : capture in progress, not allowed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    mStateLock.unlock();

    // preview size
    int new_preview_width  = 0;
    int new_preview_height = 0;
    params.getPreviewSize(&new_preview_width, &new_preview_height);
    const char * new_str_preview_format = params.getPreviewFormat();
    LOGV("%s : new_preview_width x new_preview_height = %dx%d, format = %s",
            __FUNCTION__, new_preview_width, new_preview_height, new_str_preview_format);

    if(0 < new_preview_width && 0 < new_preview_height && new_str_preview_format != NULL) {
        int new_preview_format = 0;

        if (strcmp(new_str_preview_format, "rgb565") == 0) {
            LOGD("rgb565");
            new_preview_format = V4L2_PIX_FMT_RGB565;
        } else if (strcmp(new_str_preview_format, "yuv420sp") == 0) {
            LOGD("yuv420sp");
            new_preview_format = V4L2_PIX_FMT_NV21;
        } else if (strcmp(new_str_preview_format, "yuv420sp_custom") == 0) {
            LOGD("yuv420sp_custom");
            new_preview_format = V4L2_PIX_FMT_NV12T;
        } else if (strcmp(new_str_preview_format, "yuv420sp_ycrcb") == 0) {
            LOGD("yuv420sp_ycrcb");
            new_preview_format = V4L2_PIX_FMT_NV21;
        } else if (strcmp(new_str_preview_format, CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
            LOGD("PIXEL_FORMAT_YUV420SP");
            new_preview_format = V4L2_PIX_FMT_NV21;
        } else if (strcmp(new_str_preview_format, "yuv420p") == 0) {
            LOGD("yuv420p");
            new_preview_format = V4L2_PIX_FMT_YUV420;
        } else if (strcmp(new_str_preview_format, "yuv422i") == 0) {
            LOGD("yuv422i");
            new_preview_format = V4L2_PIX_FMT_YUYV;
        } else if (strcmp(new_str_preview_format, "yuv422p") == 0) {
            LOGD("yuv422p");
            new_preview_format = V4L2_PIX_FMT_YUV422P;
        } else {
            LOGD("none = NV21");
            new_preview_format = V4L2_PIX_FMT_NV21; //for 3rd party
        }

        if(mSecCamera->setPreviewSize(new_preview_width, new_preview_height, new_preview_format) < 0) {
            LOGE("ERR(%s):Fail on mSecCamera->setPreviewSize(width(%d), height(%d), format(%d))",
                    __FUNCTION__, new_preview_width, new_preview_height, new_preview_format);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.setPreviewSize(new_preview_width, new_preview_height);
            mParameters.setPreviewFormat(new_str_preview_format);
        }

#if defined(BOARD_USES_CAMERA_OVERLAY)
        if (mUseOverlay == true && mOverlay != 0) {
            if (mOverlay->setCrop(0, 0, new_preview_width, new_preview_height) != NO_ERROR) {
                LOGE("ERR(%s)::(mOverlay->setCrop(0, 0, %d, %d) fail",
                        __FUNCTION__, new_preview_width, new_preview_height);
            }
        }
#endif
    }

    int new_picture_width  = 0;
    int new_picture_height = 0;
    int new_picture_size = 0;
    mParameters.getPictureSize(&new_picture_width, &new_picture_height);
    if(0 < new_picture_width && 0 < new_picture_height) {
        if(mSecCamera->setSnapshotSize(new_picture_width, new_picture_height) < 0) {
            LOGE("ERR(%s):Fail on mSecCamera->setSnapshotSize(width(%d), height(%d))",
                    __FUNCTION__, new_picture_width, new_picture_height);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.setPictureSize(new_picture_width, new_picture_height);
        }
    }

    // picture format
    const char * new_str_picture_format = mParameters.getPictureFormat();
    if(new_str_picture_format != NULL) {
        int new_picture_format = 0;

        if (strcmp(new_str_picture_format, "rgb565") == 0)
            new_picture_format = V4L2_PIX_FMT_RGB565;
        else if (strcmp(new_str_picture_format, "yuv420sp") == 0)
            new_picture_format = V4L2_PIX_FMT_NV21; //Kamat: Default format
        else if (strcmp(new_str_picture_format, "yuv420sp_custom") == 0)
            new_picture_format = V4L2_PIX_FMT_NV12T;
        else if (strcmp(new_str_picture_format, "yuv420sp_ycrcb") == 0)
            new_picture_format = V4L2_PIX_FMT_NV21; //Kamat: Default format
        else if (strcmp(new_str_picture_format, "yuv420p") == 0)
            new_picture_format = V4L2_PIX_FMT_YUV420;
        else if (strcmp(new_str_picture_format, "yuv422i") == 0)
            new_picture_format = V4L2_PIX_FMT_YUYV;
        else if (strcmp(new_str_picture_format, "uyv422i") == 0)
            new_picture_format = V4L2_PIX_FMT_UYVY;
        else if (!strcmp(new_str_picture_format, CameraParameters::PIXEL_FORMAT_JPEG))
            new_picture_format = V4L2_PIX_FMT_YUYV;
        else if (strcmp(new_str_picture_format, "yuv422p") == 0)
            new_picture_format = V4L2_PIX_FMT_YUV422P;
        else
            new_picture_format = V4L2_PIX_FMT_NV21; //for 3rd party

        if(mSecCamera->setSnapshotPixelFormat(new_picture_format) < 0) {
            LOGE("ERR(%s):Fail on mSecCamera->setSnapshotPixelFormat(format(%d))",
                    __FUNCTION__, new_picture_format);
            ret = UNKNOWN_ERROR;
        } else
            mParameters.setPictureFormat(new_str_picture_format);
    }

    mSecCamera->getSnapshotSize(&new_picture_width, &new_picture_height, &new_picture_size);

    int rawHeapSize = new_picture_size;
    LOGV("mRawHeap : MemoryHeapBase(previewHeapSize(%d))", rawHeapSize);
    mRawHeap = new MemoryHeapBase(rawHeapSize);
    if (mRawHeap->getHeapID() < 0) {
        LOGE("ERR(%s): Raw heap creation fail", __FUNCTION__);
        mRawHeap.clear();
    }

    //JPEG image quality
    int new_jpeg_quality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    LOGV("%s : new_jpeg_quality %d", __FUNCTION__, new_jpeg_quality);
    /* we ignore bad values */
    if (new_jpeg_quality >=1 && new_jpeg_quality <= 100) {
        mSecCamera->setJpegQuality(new_jpeg_quality);
        LOGV("(%s):mSecCamera->setJpegQuality(quality(%d))", __FUNCTION__, new_jpeg_quality);
        mParameters.set(CameraParameters::KEY_JPEG_QUALITY, new_jpeg_quality);
    }


    // JPEG thumbnail size
    int new_jpeg_thumbnail_width = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int new_jpeg_thumbnail_height= params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    if (0 <= new_jpeg_thumbnail_width && 0 <= new_jpeg_thumbnail_height) {
        if (mSecCamera->setJpegThumbnailSize(new_jpeg_thumbnail_width, new_jpeg_thumbnail_height) < 0) {
            LOGE("ERR(%s):Fail on mSecCamera->setJpegThumbnailSize(width(%d), height(%d))",
                    __FUNCTION__, new_jpeg_thumbnail_width, new_jpeg_thumbnail_height);
            ret = UNKNOWN_ERROR;
        } else {
            mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, new_jpeg_thumbnail_width);
            mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, new_jpeg_thumbnail_height);
        }
    }

    // frame rate
    int new_frame_rate = params.getPreviewFrameRate();
    if(new_frame_rate < 5 || new_frame_rate > 30)
        new_frame_rate = 30;

    mParameters.setPreviewFrameRate(new_frame_rate);
    // Calculate how long to wait between frames.
    mPreviewFrameRateMicrosec = (int)(1000000.0f / float(new_frame_rate));

    LOGV("frame rate:%d, mPreviewFrameRateMicrosec:%d", new_frame_rate, mPreviewFrameRateMicrosec);
    mSecCamera->setFrameRate(new_frame_rate);

    // rotation
    int new_rotation = params.getInt(CameraParameters::KEY_ROTATION);
    LOGV("%s : new_rotation %d", __FUNCTION__, new_rotation);
    if(new_rotation != -1) {
        if(mSecCamera->SetRotate(new_rotation) < 0) {
            LOGE("%s::mSecCamera->SetRotate(%d) fail", __FUNCTION__, new_rotation);
            ret = UNKNOWN_ERROR;
            mParameters.set(CameraParameters::KEY_ROTATION, new_rotation);
        }
    }

    // brightness

    // whitebalance

    // scene mode
    const char * new_scene_mode_str = params.get("scene-mode");
    int new_scene_mode = -1;
    if(new_scene_mode_str != NULL) {
        if(strcmp(new_scene_mode_str, "auto") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_AUTO;
        else if(strcmp(new_scene_mode_str, "beach") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_BEACH;
        else if(strcmp(new_scene_mode_str, "candlelight") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_CANDLELIGHT;
        else if(strcmp(new_scene_mode_str, "fireworks") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_FIREWORKS;
        else if(strcmp(new_scene_mode_str, "landscape") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_LANDSCAPE;
        else if(strcmp(new_scene_mode_str, "night") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_NIGHT;
        else if(strcmp(new_scene_mode_str, "night-portrait") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_NIGHTPORTRAIT;
        else if(strcmp(new_scene_mode_str, "party") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_PARTY;
        else if(strcmp(new_scene_mode_str, "portrait") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_PORTRAIT;
        else if(strcmp(new_scene_mode_str, "snow") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_SNOW;
        else if(strcmp(new_scene_mode_str, "sports") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_SPORTS;
        else if(strcmp(new_scene_mode_str, "steadyphoto") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_STEADYPHOTO;
        else if(strcmp(new_scene_mode_str, "sunset") == 0)
            new_scene_mode = SecCamera::SCENE_MODE_SUNSET;
        else
            LOGE("%s::unmatched scene-mode(%s)", __FUNCTION__, new_scene_mode_str);

        if(0 <= new_scene_mode) {		    // set scene mode
            if(mSecCamera->setSceneMode(new_scene_mode) < 0) {
                LOGE("ERR(%s):Fail on mSecCamera->setSceneMode(new_scene_mode(%d))", __FUNCTION__, new_scene_mode);
                ret = UNKNOWN_ERROR;
            }
        }
    }

    // fps range
#if 0
    // Recording size
    int new_recording_width = mParameters.getInt("recording-size-width");
    int new_recording_height= mParameters.getInt("recording-size-height");

    if (0 < new_recording_width && 0 < new_recording_height) {
        if (mSecCamera->setRecordingSize(new_recording_width, new_recording_height) < 0) {
            LOGE("ERR(%s):Fail on mSecCamera->setRecordingSize(width(%d), height(%d))",
                    __FUNCTION__, new_recording_width, new_recording_height);
            ret = UNKNOWN_ERROR;
        }
    } else {
        if (mSecCamera->setRecordingSize(new_preview_width, new_preview_height) < 0) {
            LOGE("ERR(%s):Fail on mSecCamera->setRecordingSize(width(%d), height(%d))",
                    __FUNCTION__, new_preview_width, new_preview_height);
            ret = UNKNOWN_ERROR;
        }
    }
#endif

    LOGV("--%s : ret = %d", __FUNCTION__, ret);

    return ret;
}

CameraParameters CameraHardwareSec::getParameters() const
{
    LOGV("%s :", __FUNCTION__);
    return mParameters;
}

status_t CameraHardwareSec::sendCommand(int32_t command, int32_t arg1,
                                         int32_t arg2)
{
    int  cur_zoom     = 0;
    int  new_zoom     = 0;
    bool enable_zoom  = false;
    bool flag_stopped = false;
    int  min_zoom     = 0;
    int  max_zoom     = 10;
    LOGV("++%s :", __FUNCTION__);

    switch(command) {
        case CAMERA_CMD_START_SMOOTH_ZOOM :
            LOGD("%s::CAMERA_CMD_START_SMOOTH_ZOOM arg1(%d) arg2(%d)\n", __FUNCTION__, arg1, arg2);

            min_zoom = mSecCamera->getZoomMin();
            max_zoom = mSecCamera->getZoomMax();
            cur_zoom = mSecCamera->getZoom();
            new_zoom = arg1;

            if(cur_zoom < new_zoom && cur_zoom < max_zoom)
                enable_zoom = true;
            else if(new_zoom < cur_zoom && min_zoom < cur_zoom)
                enable_zoom = true;

            if(enable_zoom == true && mSecCamera->setZoom(new_zoom) < 0) {
                LOGE("ERR(%s):Fail on mSecCamera->setZoom(new_zoom(%d))", __FUNCTION__, new_zoom);
                enable_zoom = false;
            }

            if(enable_zoom == false
                    || new_zoom == max_zoom
                    || new_zoom == min_zoom) {
                flag_stopped = true;
            } else
                flag_stopped = false;

            // kcoolsw : we need to check up..
            flag_stopped = true;

            if(mNotifyCb && (mMsgEnabled & CAMERA_MSG_ZOOM))
                mNotifyCb(CAMERA_MSG_ZOOM, new_zoom, flag_stopped, mCallbackCookie);

            break;

        case CAMERA_CMD_STOP_SMOOTH_ZOOM :
            LOGD("%s::CAMERA_CMD_STOP_SMOOTH_ZOOM  arg1(%d) arg2(%d)\n", __FUNCTION__, arg1, arg2);
            break;

        case CAMERA_CMD_SET_DISPLAY_ORIENTATION :
        default :
            LOGD("%s::default command(%d) arg1(%d) arg2(%d)\n", __FUNCTION__, command, arg1, arg2);
            break;
    }
    LOGV("--%s :", __FUNCTION__);

    //return BAD_VALUE;
    return NO_ERROR;
}

void CameraHardwareSec::release()
{
    LOGV("++%s :", __FUNCTION__);
#if defined(BOARD_USES_CAMERA_OVERLAY)
    if (mUseOverlay) {
        mOverlay->destroy();
        mUseOverlay = false;
        mOverlay = NULL;
    }
#endif

    sp<PreviewThread> previewThread;

    {
        Mutex::Autolock autofocusLock(&mAutofocusLock);

        previewThread = mPreviewThread;
    }

    if (previewThread != 0)
        previewThread->requestExitAndWait();

    {
        Mutex::Autolock autofocusLock(&mAutofocusLock);

        mPreviewThread.clear();

        if(mSecCamera != NULL) {
            mSecCamera->Destroy();
            mSecCamera = NULL;
        }

        if(mRawHeap != NULL)
            mRawHeap.clear();

        if(mJpegHeap != NULL)
            mJpegHeap.clear();

        if(mPreviewHeap != NULL)
            mPreviewHeap.clear();

        if(mRecordHeap != NULL)
            mRecordHeap.clear();

        if(mRawAddrHeap != NULL)
            mRawAddrHeap.clear();
    }
    LOGV("--%s :", __FUNCTION__);
}

status_t CameraHardwareSec::dump(int fd, const Vector<String16>& args) const
{
#if 0
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    AutoMutex cameraLock(&mCameraLock);
    if (mSecCamera != 0) {
        mSecCamera->dump(fd, args);
        mParameters.dump(fd, args);
        snprintf(buffer, 255, " preview frame(%d), size (%d), running(%s)\n",
                mCurrentPreviewFrame, mPreviewFrameSize, mPreviewRunning?"true": "false");
        result.append(buffer);
    } else {
        result.append("No camera client yet.\n");
    }
    write(fd, result.string(), result.size());
#endif
    return NO_ERROR;
}

// ---------------------------------------------------------------------------

int CameraHardwareSec::m_beginAutoFocusThread(void *cookie)
{
    LOGV("%s :", __FUNCTION__);
    CameraHardwareSec *c = (CameraHardwareSec *)cookie;
    return c->m_autoFocusThreadFunc();
}

int CameraHardwareSec::m_beginPictureThread(void *cookie)
{
    LOGV("%s :", __FUNCTION__);
    CameraHardwareSec *c = (CameraHardwareSec *)cookie;
    return c->pictureThread();
}

int CameraHardwareSec::m_getGpsInfo(CameraParameters * camParams, gps_info * gps)
{
    int flag_gps_info_valid = 1;
    int each_info_valid = 1;

    if(camParams == NULL || gps == NULL)
        return -1;

#define PARSE_LOCATION(what,type,fmt,desc)                              \
    do {                                                                \
        const char *what##_str = camParams->get("gps-"#what);           \
        if (what##_str) {                                               \
            type what = 0;                                              \
            if (sscanf(what##_str, fmt, &what) == 1)                    \
                gps->what = what;                                       \
            else {                                                      \
                LOGE("GPS " #what " %s could not"                       \
                        " be parsed as a " #desc,                       \
                        what##_str);                                    \
                each_info_valid = 0;                                    \
            }                                                           \
        } else                                                          \
            each_info_valid = 0;                                        \
    } while(0)

    PARSE_LOCATION(latitude,  double, "%lf", "double float");
    PARSE_LOCATION(longitude, double, "%lf", "double float");

    if(each_info_valid == 0)
        flag_gps_info_valid = 0;

    PARSE_LOCATION(timestamp, long, "%ld", "long");
    if (!gps->timestamp)
        gps->timestamp = time(NULL);

    PARSE_LOCATION(altitude, int, "%d", "int");

#undef PARSE_LOCATION

    if(flag_gps_info_valid == 1)
        return 0;
    else
        return -1;
}


#ifdef RUN_VARIOUS_EFFECT_TEST
void CameraHardwareSec::m_runEffectTest(void)
{
    int cur_zoom  = 1;
    static int next_zoom = 1;

    // zoom test
    cur_zoom = mSecCamera->getZoom();
    if(cur_zoom == SecCamera::ZOOM_BASE)
        next_zoom = 1;
    if(cur_zoom == SecCamera::ZOOM_MAX)
        next_zoom = -1;
    mSecCamera->setZoom(cur_zoom + next_zoom);

    // scene_test : portrait mode die..
    int cur_scene  = 1;
    static int next_scene = 1;
    cur_scene = mSecCamera->getSceneMode();
    if(cur_scene == SecCamera::SCENE_MODE_BASE +1)
        next_scene = 1;
    if(cur_scene == SecCamera::SCENE_MODE_MAX  -1)
        next_scene = -1;
    mSecCamera->setSceneMode(cur_scene + next_scene);

    // wb test
    int cur_wb  = 1;
    static int next_wb = 1;
    cur_wb = mSecCamera->getWhiteBalance();
    if(cur_wb == SecCamera::WHITE_BALANCE_BASE +1)
        next_wb = 1;
    if(cur_wb == SecCamera::WHITE_BALANCE_MAX  -1)
        next_wb = -1;
    mSecCamera->setWhiteBalance(cur_wb + next_wb);

    // imageeffect mode
    int cur_effect = 1;
    static int next_effect = 1;
    // color effect
    cur_effect = mSecCamera->getImageEffect();
    if(cur_effect == SecCamera::IMAGE_EFFECT_BASE +1)
        next_effect = 1;
    if(cur_effect == SecCamera::IMAGE_EFFECT_MAX  -1)
        next_effect = -1;
    mSecCamera->setImageEffect(cur_effect + next_effect);

    // brightness : die..
    int cur_br = 1;
    static int next_br = 1;

    cur_br = mSecCamera->getBrightness();
    if(cur_br == SecCamera::BRIGHTNESS_BASE)
        next_br = 1;
    if(cur_br == SecCamera::BRIGHTNESS_MAX)
        next_br = -1;
    mSecCamera->setBrightness(cur_br + next_br);

    // contrast test
    int cur_cont = 1;
    static int next_cont= 1;
    cur_cont = mSecCamera->getContrast();
    if(cur_cont == SecCamera::CONTRAST_BASE)
        next_cont = 1;
    if(cur_cont == SecCamera::CONTRAST_MAX)
        next_cont = -1;
    mSecCamera->setContrast(cur_cont + next_cont);

    // saturation
    int cur_sat = 1;
    static int next_sat= 1;
    cur_sat = mSecCamera->getSaturation();
    if(cur_sat == SecCamera::SATURATION_BASE)
        next_sat = 1;
    if(cur_sat == SecCamera::SATURATION_MAX)
        next_sat = -1;
    mSecCamera->setSaturation(cur_sat + next_sat);

    // sharpness
    int cur_sharp = 1;
    static int next_sharp= 1;
    cur_sharp = mSecCamera->getSharpness();
    if(cur_sharp == SecCamera::SHARPNESS_BASE)
        next_sharp = 1;
    if(cur_sharp == SecCamera::SHARPNESS_MAX)
        next_sharp = -1;
    mSecCamera->setSharpness(cur_sharp + next_sharp);

    // metering
    int cur_met = 1;
    static int next_met= 1;
    cur_met = mSecCamera->getMetering();
    if(cur_met == SecCamera::METERING_BASE   +1)
        next_met = 1;
    if(cur_met == SecCamera::METERING_MAX    -1)
        next_met = -1;
    mSecCamera->setMetering(cur_met + next_met);
    //
}
#endif // RUN_VARIOUS_EFFECT_TEST


wp<CameraHardwareInterface> CameraHardwareSec::singleton;

sp<CameraHardwareInterface> CameraHardwareSec::createInstance(int cameraId)
{
    LOGV("%s :", __FUNCTION__);
    if (singleton != 0) {
        sp<CameraHardwareInterface> hardware = singleton.promote();
        if (hardware != 0) {
            return hardware;
        }
    }
    sp<CameraHardwareInterface> hardware(new CameraHardwareSec(cameraId));
    singleton = hardware;
    return hardware;
}

static CameraInfo sCameraInfo[] = {
    {
        CAMERA_FACING_BACK,
        270 /* orientation */
    }, {
        CAMERA_FACING_FRONT,
        0,  /* orientation */
    }
};

extern "C" int HAL_getNumberOfCameras()
{
    return sizeof(sCameraInfo) / sizeof(sCameraInfo[0]);
}

extern "C" void HAL_getCameraInfo(int cameraId, struct CameraInfo *cameraInfo)
{
    memcpy(cameraInfo, &sCameraInfo[cameraId], sizeof(CameraInfo));
}

extern "C" sp<CameraHardwareInterface> HAL_openCameraHardware(int cameraId)
{
    LOGV("%s :", __FUNCTION__);
    return CameraHardwareSec::createInstance(cameraId);
}

}; // namespace android
