/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "SecHardwareRenderer"
#define LOG_NDEBUG 0
#include <utils/Log.h>

#include "SecHardwareRenderer.h"

#include <media/stagefright/MediaDebug.h>
#include <surfaceflinger/ISurface.h>
#include <ui/Overlay.h>

#include <hardware/hardware.h>

#include <linux/fb.h>
#include <fcntl.h>

#include "v4l2_utils.h"
#include "utils/Timers.h"
#include "sec_base.h"

#define CACHEABLE_BUFFERS 0x1

#define USE_ZERO_COPY
//#define SEC_DEBUG

#ifdef USE_SAMSUNG_COLORFORMAT
#define OMX_SEC_COLOR_FormatNV12TPhysicalAddress        0x7F000001
#endif

namespace android {


static int  mNumOfPlayingContents = 0;
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////

SecHardwareRenderer::SecHardwareRenderer(
        const sp<ISurface> &surface,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        OMX_COLOR_FORMATTYPE colorFormat,
        int32_t rotationDegrees,
        bool fromHardwareDecoder)
    : mISurface(surface),
      mDisplayWidth(displayWidth),
      mDisplayHeight(displayHeight),
      mDecodedWidth(decodedWidth),
      mDecodedHeight(decodedHeight),
      mColorFormat(colorFormat),
      mInitCheck(NO_INIT),
      mFrameSize(mDecodedWidth * mDecodedHeight * 2),
      mIsFirstFrame(true),
      mCustomFormat(fromHardwareDecoder),
      mUseOverlay(true),
      mIndex(0) {

    status_t ret;

    CHECK(mISurface.get() != NULL);
    CHECK(mDecodedWidth > 0);
    CHECK(mDecodedHeight > 0);

    if (colorFormat != OMX_COLOR_FormatCbYCrY
            && colorFormat != OMX_COLOR_FormatYUV420Planar
#ifdef USE_SAMSUNG_COLORFORMAT
            && colorFormat != OMX_SEC_COLOR_FormatNV12TPhysicalAddress
#endif
            && colorFormat != OMX_COLOR_FormatYUV420SemiPlanar) {
        LOGE("Invalid colorFormat (0x%x)", colorFormat);
        return;
    }

    switch (rotationDegrees) {
        case 0: mOrientation = ISurface::BufferHeap::ROT_0; break;
        case 90: mOrientation = ISurface::BufferHeap::ROT_90; break;
        case 180: mOrientation = ISurface::BufferHeap::ROT_180; break;
        case 270: mOrientation = ISurface::BufferHeap::ROT_270; break;
        default: mOrientation = ISurface::BufferHeap::ROT_0; break;
    }

    pthread_mutex_lock(&lock);
    if(++mNumOfPlayingContents > 1)
    mUseOverlay = false;
    pthread_mutex_unlock(&lock);

    if (mUseOverlay)
        ret = initializeOverlaySource();
    else
        ret = initializeBufferSource();

    if (ret != OK)
        return;
    mInitCheck = OK;
}

status_t SecHardwareRenderer::initializeOverlaySource() {
    sp<OverlayRef> ref;

#if defined (USE_ZERO_COPY)
    if (mCustomFormat == true ) {
        ref = mISurface->createOverlay(
                mDecodedWidth, mDecodedHeight,
                HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP, mOrientation);
    } else
#else
        ref = mISurface->createOverlay(
                mDecodedWidth, mDecodedHeight, HAL_PIXEL_FORMAT_YCbCr_420_P,
                mOrientation);
#endif

        if (ref.get() == NULL) {
            LOGE("Unable to create the overlay!");
            pthread_mutex_lock(&lock);
            mNumOfPlayingContents--;
            pthread_mutex_unlock(&lock);
            return FAILED_TRANSACTION;
        }

        mOverlay = new Overlay(ref);
        mOverlay->setParameter(CACHEABLE_BUFFERS, 0);

        mNumBuf = mOverlay->getBufferCount();

        if (mCustomFormat) {
            mFrameSize = sizeof(struct ADDRS);
            mMemoryHeap = new MemoryHeapBase(mFrameSize * mNumBuf);
        } else {
            for (size_t i = 0; i < (size_t)mNumBuf; ++i) {
                void *addr = mOverlay->getBufferAddress((void *)i);
                mOverlayAddresses.push(addr);
            }
        }

     mIndex = 0;

    return OK;
}


status_t SecHardwareRenderer::initializeBufferSource() {

#ifndef BOARD_USES_COPYBIT
    return INVALID_OPERATION;
#endif

    if (!mCustomFormat) {
        mMemoryHeap = new MemoryHeapBase("/dev/pmem_adsp", 2 * mFrameSize);
        if (mMemoryHeap->heapID() >= 0) {
            sp<MemoryHeapPmem> pmemHeap = new MemoryHeapPmem(mMemoryHeap);
            pmemHeap->slap();
            mMemoryHeap.clear();
            mMemoryHeap = pmemHeap;
        }
    } else
            mFrameSize = 32;

    if (mMemoryHeap.get() == NULL || mMemoryHeap->heapID() < 0)
            mMemoryHeap = new MemoryHeapBase(2 * mFrameSize);

    CHECK(mMemoryHeap->heapID() >= 0);

        ISurface::BufferHeap bufferHeap(
                mDisplayWidth, mDisplayHeight,
                mDecodedWidth, mDecodedHeight,
            mCustomFormat?HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:HAL_PIXEL_FORMAT_YCbCr_420_P,
            mOrientation, 0,
                mMemoryHeap);

        status_t err = mISurface->registerBuffers(bufferHeap);
    if (err != OK) {
        LOGE("ISurface failed to register buffers (0x%08x)", err);
        pthread_mutex_lock(&lock);
        mNumOfPlayingContents--;
        pthread_mutex_unlock(&lock);
        return err;
    }

    mIndex = 0;

    return OK;
}


SecHardwareRenderer::~SecHardwareRenderer() {

    pthread_mutex_lock(&lock);
    if(--mNumOfPlayingContents < 0)
        mNumOfPlayingContents = 0;
    pthread_mutex_unlock(&lock);

    if (mUseOverlay == false)
        deinitializeBufferSource();
    else
        deinitializeOverlaySource();
}

void SecHardwareRenderer::deinitializeOverlaySource() {

    if (mOverlay.get() != NULL) {
        mOverlay->destroy();
        mOverlay.clear();
    }

    if(mMemoryHeap != NULL)
        mMemoryHeap.clear();
}

void SecHardwareRenderer::deinitializeBufferSource() {

#ifndef BOARD_USES_COPYBIT
    return;
#endif

       mISurface->unregisterBuffers();

    if(mMemoryHeap != NULL)
        mMemoryHeap.clear();
}

void SecHardwareRenderer::handleYUV420Planar(
        const void *src_data, void *dst_data, size_t size) {

    // first int is frame_size : overlay doesn't need it.
    //memcpy(&FrameSize, data,  sizeof(int));
    struct ADDRS *src_addrs = (struct ADDRS *)(src_data + sizeof(int));
    struct ADDRS *dst_addrs = (struct ADDRS *)(dst_data);

    dst_addrs->addr_y    = src_addrs->addr_y;
    dst_addrs->addr_cbcr = src_addrs->addr_cbcr;
    dst_addrs->buf_idx   = mIndex;
}

void SecHardwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {

     status_t ret = OK;

    if (mInitCheck != OK)
            return;

    if (mUseOverlay == true && mOverlay.get() == NULL)
        return;

  if (mNumOfPlayingContents > 1 && mUseOverlay == true) {
        mUseOverlay = false;

        deinitializeOverlaySource();

        ret = initializeBufferSource();
        if (ret != OK)
                    return;

                    mIsFirstFrame = true;
                }

    if (mUseOverlay) {
        overlay_buffer_t dst;

    if (mCustomFormat) {
        /* zero copy solution case */
        dst = (uint8_t *)mMemoryHeap->getBase() + (mFrameSize * mIndex);

        if (mColorFormat == OMX_COLOR_FormatYUV420Planar ||
#ifdef USE_SAMSUNG_COLORFORMAT
            mColorFormat == OMX_SEC_COLOR_FormatNV12TPhysicalAddress ||
#endif
            mColorFormat == OMX_COLOR_FormatYUV420SemiPlanar) {
            handleYUV420Planar(data, dst, size);
            }
        } else {
            /* normal frame case */
            dst = (void *)mIndex;
                memcpy(mOverlayAddresses[mIndex], data, size);
            }

        if (mOverlay->queueBuffer(dst) == ALL_BUFFERS_FLUSHED) {
                mIsFirstFrame = true;
           if (mOverlay->queueBuffer((void *)dst) != 0) {
                    return;
                }
            }

            if (++mIndex == mNumBuf) {
                mIndex = 0;
            }

            overlay_buffer_t overlay_buffer;
            if (!mIsFirstFrame) {
                status_t err = mOverlay->dequeueBuffer(&overlay_buffer);
                if (err == ALL_BUFFERS_FLUSHED) {
                    mIsFirstFrame = true;
                } else {
                    return;
                }
            } else {
                mIsFirstFrame = false;
            }
    } else {
        size_t offset = mIndex * mFrameSize;
        void *dst = (uint8_t *)mMemoryHeap->getBase() + offset;

        if (mCustomFormat)
               handleYUV420Planar(data, dst, size);
        else
            memcpy(dst, data, size);

        mISurface->postBuffer(offset);
        mIndex = 1 - mIndex;
    }
}

}  // namespace android

