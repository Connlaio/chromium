/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DeferredImageDecoder_h
#define DeferredImageDecoder_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkRWBuffer.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

class SkImage;

namespace blink {

class ImageFrameGenerator;
class SharedBuffer;
struct FrameData;

class PLATFORM_EXPORT DeferredImageDecoder final {
    WTF_MAKE_NONCOPYABLE(DeferredImageDecoder);
    USING_FAST_MALLOC(DeferredImageDecoder);
public:
    static PassOwnPtr<DeferredImageDecoder> create(const SharedBuffer& data, ImageDecoder::AlphaOption, ImageDecoder::GammaAndColorProfileOption);

    static PassOwnPtr<DeferredImageDecoder> createForTesting(PassOwnPtr<ImageDecoder>);

    ~DeferredImageDecoder();

    static void setEnabled(bool);
    static bool enabled();

    String filenameExtension() const;

    PassRefPtr<SkImage> createFrameAtIndex(size_t);

    void setData(SharedBuffer& data, bool allDataReceived);

    bool isSizeAvailable();
    bool hasColorProfile() const;
    IntSize size() const;
    IntSize frameSizeAtIndex(size_t index) const;
    size_t frameCount();
    int repetitionCount() const;
    size_t clearCacheExceptFrame(size_t index);
    bool frameHasAlphaAtIndex(size_t index) const;
    bool frameIsCompleteAtIndex(size_t index) const;
    float frameDurationAtIndex(size_t index) const;
    size_t frameBytesAtIndex(size_t index) const;
    ImageOrientation orientationAtIndex(size_t index) const;
    bool hotSpot(IntPoint&) const;

private:
    explicit DeferredImageDecoder(PassOwnPtr<ImageDecoder> actualDecoder);

    friend class DeferredImageDecoderTest;
    ImageFrameGenerator* frameGenerator() { return m_frameGenerator.get(); }

    void activateLazyDecoding();
    void prepareLazyDecodedFrames();

    PassRefPtr<SkImage> createFrameImageAtIndex(size_t index, bool knownToBeOpaque) const;

    // Copy of the data that is passed in, used by deferred decoding.
    // Allows creating readonly snapshots that may be read in another thread.
    OwnPtr<SkRWBuffer> m_rwBuffer;
    bool m_allDataReceived;
    OwnPtr<ImageDecoder> m_actualDecoder;

    String m_filenameExtension;
    IntSize m_size;
    int m_repetitionCount;
    bool m_hasColorProfile;
    bool m_canYUVDecode;

    // Carries only frame state and other information. Does not carry bitmap.
    Vector<FrameData> m_frameData;
    RefPtr<ImageFrameGenerator> m_frameGenerator;

    static bool s_enabled;
};

} // namespace blink

#endif
