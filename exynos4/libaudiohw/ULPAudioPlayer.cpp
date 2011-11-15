/*
 *
 * Copyright 2010 The Android Open Source Project
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
 * @file        ULPAudioPlayer.cpp
 * @brief
 * @author      Yunji Kim (yunji.kim@samsung.com)
 *              HyeYeon Chung (hyeon.chung@samsung.com)
 * @version     2.0
 * @history
 *   2010.12.17 : Create
 *   2011.01.28 : Re-design for EOS non-blocking mode
 */

#define LOG_NDEBUG 1
#define LOG_TAG "ULPAudioPlayer"
#include <utils/Log.h>
#include <dlfcn.h>

#include <binder/IPCThreadState.h>
#include <private/media/AudioTrackShared.h>
#include <media/AudioTrack.h>
#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <include/AwesomePlayer.h>

#include "ULPAudioPlayer.h"
#include "s5p-rp_api.h"    /* for RP Driver */

namespace android {

/* for RP Driver */
const unsigned int iRPIbufSize = 16*1024;
const unsigned int iRPObufSize = 9*1024;
const unsigned int iRPWriteSize = 4*1024;

ULPAudioPlayer::ULPAudioPlayer(
        const sp<MediaPlayerBase::AudioSink> &audioSink,
        AwesomePlayer *observer)
    : mAudioTrack(NULL),
      mInputBuffer(NULL),
      mSampleRate(0),
      mLatencyUs(0),
      mFrameSize(0),
      mFrameCount(0),
      mNumFramesPlayed(0),
      mSeekTimeUs(0),
      mPositionTimeMediaUs(-1),
      mPositionTimeRealUs(-1),
      mSeeking(false),
      mReachedEOS(false),
      mFinalStatus(OK),
      mStarted(false),
      mIsFirstBuffer(false),
      mFirstBufferResult(OK),
      mFirstBuffer(NULL),
      mObserver(observer) {
    status_t ret;

    isRPCreated = false;

    ret = S5P_RP_Create(S5P_RP_INIT_BLOCK_MODE);
    if (ret < 0) {
        LOGE("S5P_RP_Create failed: %d", ret);
        return;
    }

    ret = S5P_RP_Init(iRPIbufSize);
    if (ret < 0) {
        S5P_RP_Terminate();
        LOGE("S5P_RP_Init failed: %d", ret);
        return;
    }

    mULPAudioThread = new ULPAudioThread(*this);
    if (mULPAudioThread == 0) {
        LOGE("Could not create ULP-callback thread");
        S5P_RP_Deinit();
        S5P_RP_Terminate();
        return;
    }
    isRPCreated = true;

    memset(pAudioHWdata, 0, sizeof(pAudioHWdata));
    decodedFrame = 0;
    decodedFrame_old = 0;
    isRPStopped = 0;
    isRPEos = false;
    isRPEosDecode = 2; /* running */

    CodecOn = false;
    mState = INIT;
}

ULPAudioPlayer::~ULPAudioPlayer() {
    mState = STOP;

    if (mULPAudioThread != 0) {
        mULPAudioThread->requestExitAndWait();
        mULPAudioThread.clear();
    }

    if (mStarted)
        reset();

    if (isRPCreated) {
        S5P_RP_Deinit();
        S5P_RP_Terminate();
        isRPCreated = false;
    }
}

status_t ULPAudioPlayer::initCheck() const
{
    return ((mState == INIT) ? OK : NO_INIT);
}

void ULPAudioPlayer::setSource(const sp<MediaSource> &source) {
    CHECK_EQ(mSource, NULL);
    mSource = source;
}

status_t ULPAudioPlayer::start(bool sourceAlreadyStarted) {
    LOGV("start");
    CHECK(!mStarted);
    CHECK(mSource != NULL);

    status_t err;
    if (!sourceAlreadyStarted) {
        err = mSource->start();

        if (err != OK)
            return err;
    }

    /*
     * We allow an optional INFO_FORMAT_CHANGED at the very beginning
     * of playback, if there is one, getFormat below will retrieve the
     * updated format, if there isn't, we'll stash away the valid buffer
     * of data to be used on the first audio callback.
     */
    CHECK(mFirstBuffer == NULL);

    mFirstBufferResult = mSource->read(&mFirstBuffer);
    if (mFirstBufferResult == INFO_FORMAT_CHANGED) {
        CHECK(mFirstBuffer == NULL);
        mFirstBufferResult = OK;
        mIsFirstBuffer = false;
    } else {
        mIsFirstBuffer = true;
    }

    sp<MetaData> format = mSource->getFormat();
    const char *mime;
    bool success = format->findCString(kKeyMIMEType, &mime);
    CHECK(success);
    CHECK(!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MPEG));

    success = format->findInt32(kKeySampleRate, &mSampleRate);
    CHECK(success);

    int32_t numChannels;
    success = format->findInt32(kKeyChannelCount, &numChannels);
    CHECK(success);

    mAudioTrack = new AudioTrack(
            AudioSystem::MUSIC, mSampleRate, AudioSystem::PCM_16_BIT,
            (numChannels == 2)
                ? AudioSystem::CHANNEL_OUT_STEREO
                : AudioSystem::CHANNEL_OUT_MONO,
            0, 0, NULL, this, 0);

    if ((err = mAudioTrack->initCheck()) != OK) {
        delete mAudioTrack;
        mAudioTrack = NULL;

        if (mFirstBuffer != NULL) {
            mFirstBuffer->release();
            mFirstBuffer = NULL;
        }

        if (!sourceAlreadyStarted)
            mSource->stop();

        return err;
    }

    mLatencyUs = (int64_t)mAudioTrack->latency() * 1000;
    mFrameSize = mAudioTrack->frameSize();
    mFrameCount = mAudioTrack->frameCount();

    if (mFrameCount*mFrameSize > 128*1024)
        LOGW("Dummy buffer is too small");

    mAudioTrack->start();
    if (!CodecOn) {
        mAudioTrack->write(pAudioHWdata, sizeof(unsigned char)*mFrameCount*mFrameSize);
        CodecOn = true;
    }

    mStarted = true;

    mState = START;
    err = mULPAudioThread->run("ULPAudioThread", THREAD_PRIORITY_AUDIO_CLIENT);
    if (err != NO_ERROR) {
        LOGE("Could not run ULP-callback thread");
        return err;
    }

    return OK;
}

void ULPAudioPlayer::pause(bool playPendingSamples) {
    CHECK(mStarted);

    if (!playPendingSamples) {
        LOGV("pause");
        mState = PAUSE;

        /* codec control to close pcm */
        if (CodecOn) {
            /* send wakeup signal to AudioFlinger */
            mAudioTrack->start();
            mAudioTrack->write(pAudioHWdata, sizeof(unsigned char)*mFrameCount*mFrameSize);
            CodecOn = false;
        }
    } else {
        LOGV("stop");
        mState = STOP;

        /* codec control to close pcm */
        if (CodecOn) {
            /* send wakeup signal to AudioFlinger */
            mAudioTrack->start();
            mAudioTrack->write(pAudioHWdata, sizeof(unsigned char)*mFrameCount*mFrameSize);
            CodecOn = false;
        }
    }
}

void ULPAudioPlayer::resume() {
    LOGV("resume");
    CHECK(mStarted);

    mState = START;

        /* codec control to open pcm */
        if (!CodecOn) {
            /* send wakeup signal to AudioFlinger */
            mAudioTrack->start();
            mAudioTrack->write(pAudioHWdata, sizeof(unsigned char)*mFrameCount*mFrameSize);
            CodecOn = true;
        }

        if (mULPAudioThread->requestExitAndWait() == WOULD_BLOCK) {
            LOGE("ULPAudioPlayer::resume called from thread");
            return;
        }

        mULPAudioThread->mLock.lock();

        status_t ret = mULPAudioThread->run("ULPAudioThread", THREAD_PRIORITY_AUDIO_CLIENT);
        if (ret != NO_ERROR)
            LOGE("mULPAudioThread->run(ULPAudioThread) is failed: %d", ret);

        mULPAudioThread->mLock.unlock();
    }

void ULPAudioPlayer::reset() {
    LOGV("reset");
    CHECK(mStarted);

    if (mAudioTrack) {
        /* codec control to close pcm */
        if (CodecOn) {
            mAudioTrack->start();
            mAudioTrack->write(pAudioHWdata, sizeof(unsigned char)*mFrameCount*mFrameSize);
            CodecOn = false;
        }
        mAudioTrack->stop();

        delete mAudioTrack;
        mAudioTrack = NULL;
    }

    S5P_RP_Stop();

    /*
     * Make sure to release any buffer we hold onto so that the
     * source is able to stop().
     */
    if (mFirstBuffer != NULL) {
        mFirstBuffer->release();
        mFirstBuffer = NULL;
    }

    if (mInputBuffer != NULL) {
        LOGV("ULPAudioPlayer releasing input buffer.");

        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    /*
     * The following hack is necessary to ensure that the OMX
     * component is completely released by the time we may try
     * to instantiate it again.
     */
    wp<MediaSource> tmp = mSource;
    mSource.clear();
    while (tmp.promote() != NULL)
        usleep(1000);

    IPCThreadState::self()->flushCommands();

    mNumFramesPlayed = 0;
    mSeekTimeUs = 0;
    mPositionTimeMediaUs = -1;
    mPositionTimeRealUs = -1;
    mSeeking = false;
    mReachedEOS = false;
    mFinalStatus = OK;
    mStarted = false;

    decodedFrame = 0;
    decodedFrame_old = 0;
}

bool ULPAudioPlayer::isSeeking() {
    Mutex::Autolock autoLock(mLock);
    return mSeeking;
}

bool ULPAudioPlayer::reachedEOS(status_t *finalStatus) {
    *finalStatus = OK;

    Mutex::Autolock autoLock(mLock);
    *finalStatus = mFinalStatus;
    return mReachedEOS;
}

int64_t ULPAudioPlayer::getRealTimeUs() {
    Mutex::Autolock autoLock(mLock);
    return getRealTimeUsLocked();
}

int64_t ULPAudioPlayer::getRealTimeUsLocked() const {
    return -mLatencyUs + (mNumFramesPlayed * 1000000) / mSampleRate;
}

int64_t ULPAudioPlayer::getMediaTimeUs() {
    status_t ret;

    Mutex::Autolock autoLock(mLock);

    ret = S5P_RP_GetParams(S5P_RP_DECODED_FRAME_SIZE, &decodedFrame);
    if (ret == 0) {
        if (decodedFrame> decodedFrame_old) {
            framesWritten = decodedFrame - decodedFrame_old;
            decodedFrame_old = decodedFrame;
        } else {
            framesWritten = 0;
        }
    } else {
        LOGW("S5P_RP_GetParams is failed");
        framesWritten = 0;
    }
    mNumFramesPlayed += framesWritten;
    mPositionTimeRealUs = (mNumFramesPlayed * 1000000) / mSampleRate;

    if (mPositionTimeRealUs < 0)
        return 0;

    int64_t realTimeOffset = getRealTimeUsLocked() - mPositionTimeRealUs;
    if (realTimeOffset < 0)
        realTimeOffset = 0;

    return mSeekTimeUs + mPositionTimeRealUs + realTimeOffset;
}

bool ULPAudioPlayer::getMediaTimeMapping(
        int64_t *realtime_us, int64_t *mediatime_us) {
    Mutex::Autolock autoLock(mLock);

    *realtime_us = mPositionTimeRealUs;
    *mediatime_us = mPositionTimeMediaUs;

    return mPositionTimeRealUs != -1 && mPositionTimeMediaUs != -1;
}

status_t ULPAudioPlayer::seekTo(int64_t time_us) {
    LOGV("seekTo");
    Mutex::Autolock autoLock(mLock);

    mSeeking = true;
    mReachedEOS = false;
    mSeekTimeUs = time_us;
    mNumFramesPlayed = 0;

    return OK;
}

bool ULPAudioPlayer::processAudioBuffer(const sp<ULPAudioThread>& thread)
{
    status_t ret;

    if (mState != START) {
        LOGV("mState(%d)", mState);
        if (mState == PAUSE) {
            S5P_RP_Pause();
            mAudioTrack->pause();
            isRPEosDecode = 0;
        } else if (mState == STOP) {
            S5P_RP_Stop();
            mAudioTrack->stop();
        }
        return false; /* escape from the threadLoop */
    }

    if (isRPEos == true)
        usleep(100000);

    if (mReachedEOS) {
        if (mObserver) {
            mObserver->postAudioEOS();
        }
        return true;
    }

    size_t size_done = 0;
    size_t size_remaining = iRPWriteSize;
    while (size_remaining > 0 && mState == START) {
        MediaSource::ReadOptions options;

        {
            Mutex::Autolock autoLock(mLock);
            if (mSeeking) {
                S5P_RP_Flush();
                isRPEos = false;
                isRPStopped = 0;

                if (mIsFirstBuffer) {
                    if (mFirstBuffer != NULL) {
                        mFirstBuffer->release();
                        mFirstBuffer = NULL;
                    }
                    mIsFirstBuffer = false;
                }

                options.setSeekTo(mSeekTimeUs);

                if (mInputBuffer != NULL) {
                    mInputBuffer->release();
                    mInputBuffer = NULL;
                }

                mSeeking = false;
                if (mObserver)
                    mObserver->postAudioSeekComplete();
            }
        }

        if (mInputBuffer == NULL) {
            status_t err;

            if (mIsFirstBuffer) {
                mInputBuffer = mFirstBuffer;
                mFirstBuffer = NULL;
                err = mFirstBufferResult;
                mIsFirstBuffer = false;
            } else if (isRPEos == true) {
               mInputBuffer = NULL;
               err = ERROR_END_OF_STREAM;
            } else {
                err = mSource->read(&mInputBuffer, &options);
            }

            CHECK((err == OK && mInputBuffer != NULL)
                   || (err != OK && mInputBuffer == NULL));

            Mutex::Autolock autoLock(mLock);

            if (err != OK) {
                if (isRPEos == false) {
                    S5P_RP_Send_EOS();
                    isRPEos = true;
                }
                if (mState == START) { /* resume or start */
                     if (!isRPEosDecode)
                         isRPEosDecode = 1; /* resume */
                     else
                         isRPEosDecode = 2; /* running */
                }

                if (isRPStopped == 0) {
                    ret = S5P_RP_GetParams(S5P_RP_STOP_EOS_STATE, &isRPStopped);
                    if (ret != 0)
                        LOGE("Fail S5P_RP_STOP_EOS_STATE");
                    if (isRPStopped == 1) {
                        mReachedEOS = true;
                        mFinalStatus = err;
                        break;
                    }
                }
            } else {
                CHECK(mInputBuffer->meta_data()->findInt64(kKeyTime, &mPositionTimeMediaUs));
            }
        }

        size_t copy;
        if (isRPEos == false) {
            if (mInputBuffer->range_length() == 0) {
                mInputBuffer->release();
                mInputBuffer = NULL;

                continue;
            }

            copy= size_remaining;

            if (copy > mInputBuffer->range_length())
                copy = mInputBuffer->range_length();
        }

        if (isRPEos == false) {
            ret = S5P_RP_Decode((char *)mInputBuffer->data() + mInputBuffer->range_offset(), copy);
            isRPEosDecode = 2; /* running */
        } else if ((isRPEos == true) && (isRPEosDecode == 1)) {
            ret = S5P_RP_Resume_EOS();
            isRPEosDecode = 2; /* running */
        }

        if (ret != 0) {
            if (ret < 0)
                LOGE("S5P_RP_Decode failed");
            else
                LOGE("S5P_RP_Decode returned error code: 0x%05X", ret);
        }

        if (isRPEos == false) {
            mInputBuffer->set_range(mInputBuffer->range_offset() + copy,
                                    mInputBuffer->range_length() - copy);
        } else {
            copy = size_remaining;
        }

        size_done += copy;
        size_remaining -= copy;
    } /* end of while loop */

    return true;
}

/*
 * buffer thread function
 */
ULPAudioPlayer::ULPAudioThread::ULPAudioThread(ULPAudioPlayer& receiver, bool bCanCallJava)
    : Thread(bCanCallJava), mReceiver(receiver)
{
}

bool ULPAudioPlayer::ULPAudioThread::threadLoop()
{
    return mReceiver.processAudioBuffer(this);
}

/*
 * factory function for AwesomePlayer linkage
 */
extern "C" AudioPlayerBase * createAudioMio(const sp<MediaPlayerBase::AudioSink> &audioSink,
                                        AwesomePlayer *observer)
{
    LOGI("Creating Vendor(SEC) Specific Audio MIO for ULP");
    return new ULPAudioPlayer(audioSink, observer);
}

extern "C" void destroyAudioMio(AudioPlayerBase *audioPlayer)
{
    if (audioPlayer) {
        LOGI("Destroy Vendor(SEC) Specific Audio MIO for ULP");
        delete audioPlayer;
    }
}

} /* end of namespace */
