/*
**
** Copyright 2008, The Android Open Source Project
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

#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_SEC_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_SEC_H

#include "SecCamera.h"
#include <utils/threads.h>
#include <camera/CameraHardwareInterface.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>

//#define RUN_VARIOUS_EFFECT_TEST               // test repeately on zoom, white-balance, color-effect, etc

namespace android {

class CameraHardwareSec : public CameraHardwareInterface {

private:
	    class PreviewThread : public Thread {
		CameraHardwareSec *mHardware;
	    public:
		PreviewThread(CameraHardwareSec *hw):
#ifdef SINGLE_PROCESS
		// In single process mode this thread needs to be a java thread,
		// since we won't be calling through the binder.
		Thread(true),
#else
		Thread(false),
#endif
		mHardware(hw) { }
		virtual bool threadLoop() {
		    int ret = mHardware->previewThread();
		    // loop until we need to quit
		    if(ret == NO_ERROR)
			return true;
		    else
			return false;
		}
	    };


    typedef struct
	{
		double       latitude;   // degrees, WGS ellipsoid
		double       longitude;  // degrees
		unsigned int timestamp;  // seconds since 1/6/1980
		int          altitude;   // meters
	} gps_info;

private:
    static wp<CameraHardwareInterface> singleton;

    /* used to guard threading state */
    mutable Mutex       mStateLock;

    Mutex               mAutofocusLock;

    CameraParameters    mParameters;

    static const int    kBufferCount = MAX_BUFFERS;
    static const int    kBufferCountForRecord = MAX_BUFFERS;

    sp<MemoryHeapBase>  mPreviewHeap;
    sp<MemoryHeapBase>  mRawHeap;
    sp<MemoryHeapBase>  mRawAddrHeap;
    sp<MemoryHeapBase>  mRecordHeap;
    sp<MemoryHeapBase>  mJpegHeap;
    sp<MemoryBase>      mBuffers      [kBufferCount];
    //sp<MemoryBase>      mRecordBuffers[kBufferCountForRecord];

    SecCamera          *mSecCamera;
    bool                mPreviewRunning;
    bool                mRecordRunning;

    bool		mCaptureInProgress;

    unsigned int        mPreviewFrameRateMicrosec;
    DurationTimer       mFrameRateTimer;

    // protected by mCameraLock
    sp<PreviewThread>   mPreviewThread;

#if defined(BOARD_USES_CAMERA_OVERLAY)
    sp<Overlay>         mOverlay;
    bool                mUseOverlay;
    int                 mOverlayBufferIdx;
#endif

#if defined(BOARD_USES_HDMI)
    sp<ISurface>        mISurface;
#endif

    notify_callback     mNotifyCb;
    data_callback       mDataCb;
    data_callback_timestamp mDataCbTimestamp;
    void *              mCallbackCookie;

    int32_t             mMsgEnabled;

    int                 mAFMode;

    gps_info            mGpsInfo;

public:
    virtual sp<IMemoryHeap> getPreviewHeap() const;
    virtual sp<IMemoryHeap> getRawHeap() const;

//Kamat --eclair
    virtual void        setCallbacks(notify_callback notify_cb,
                                     data_callback data_cb,
                                     data_callback_timestamp data_cb_timestamp,
                                     void* user);

    virtual void        enableMsgType(int32_t msgType);
    virtual void        disableMsgType(int32_t msgType);
    virtual bool        msgTypeEnabled(int32_t msgType);

#if defined(BOARD_USES_CAMERA_OVERLAY)
    virtual bool        useOverlay();
    virtual status_t    setOverlay(const sp<Overlay> &overlay);
#endif

#if defined(BOARD_USES_HDMI)
    virtual status_t    setSurface(const sp<ISurface> &surface);
#endif

    virtual status_t    startPreview();
    virtual void        stopPreview();
    virtual bool        previewEnabled();

    virtual status_t    startRecording();
    virtual void        stopRecording();
    virtual bool        recordingEnabled();
    virtual void        releaseRecordingFrame(const sp<IMemory>& mem);

    virtual status_t    autoFocus();
    virtual status_t    cancelAutoFocus();

    virtual status_t    takePicture();
    virtual status_t    cancelPicture();

    virtual status_t    setParameters(const CameraParameters& params);
    virtual CameraParameters  getParameters() const;
    virtual status_t    sendCommand(int32_t command, int32_t arg1,
                                    int32_t arg2);

    virtual void        release();
    virtual status_t    dump(int fd, const Vector<String16>& args) const;

    static sp<CameraHardwareInterface> createInstance(int cameraId);

private:
               CameraHardwareSec(int cameraId);
    virtual    ~CameraHardwareSec();

    void       m_initDefaultParameters(int cameraId);

    int        previewThread();

    static int m_beginAutoFocusThread(void *cookie);
    int        m_autoFocusThreadFunc();

    static int m_beginPictureThread(void *cookie);
    int        pictureThread();

    int        m_getGpsInfo(CameraParameters * camParams, gps_info * gps);

    #ifdef RUN_VARIOUS_EFFECT_TEST
        void       m_runEffectTest(void);
    #endif // RUN_VARIOUS_EFFECT_TEST

    struct timeval      mTimeStart;
    struct timeval      mTimeStop;
};

}; // namespace android

#endif
