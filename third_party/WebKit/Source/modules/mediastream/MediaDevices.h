// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDevices_h
#define MediaDevices_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"

namespace blink {

class MediaStreamConstraints;
class MediaTrackSupportedConstraints;
class ScriptState;

class MediaDevices final : public GarbageCollected<MediaDevices>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaDevices* create()
    {
        return new MediaDevices();
    }

    ScriptPromise enumerateDevices(ScriptState*);
    void getSupportedConstraints(MediaTrackSupportedConstraints& result) { }
    ScriptPromise getUserMedia(ScriptState*, const MediaStreamConstraints&, ExceptionState&);
    DEFINE_INLINE_TRACE() { }

private:
    MediaDevices() { }
};

} // namespace blink

#endif
