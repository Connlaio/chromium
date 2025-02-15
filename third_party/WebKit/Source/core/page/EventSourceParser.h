// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventSourceParser_h
#define EventSourceParser_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextCodec.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT EventSourceParser final : public GarbageCollectedFinalized<EventSourceParser> {
public:
    class CORE_EXPORT Client : public GarbageCollectedMixin {
    public:
        virtual ~Client() {}
        virtual void onMessageEvent(const AtomicString& type, const String& data, const AtomicString& lastEventId) = 0;
        virtual void onReconnectionTimeSet(unsigned long long reconnectionTime) = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() {}
    };

    EventSourceParser(const AtomicString& lastEventId, Client*);

    void addBytes(const char*, size_t);
    const AtomicString& lastEventId() const { return m_lastEventId; }
    // Stop parsing. This can be called from Client::onMessageEvent.
    void stop() { m_isStopped = true; }
    DECLARE_TRACE();

private:
    void parseLine();
    String fromUTF8(const char* bytes, size_t);

    Vector<char> m_line;

    AtomicString m_eventType;
    Vector<char> m_data;
    // This variable corresponds to "last event ID buffer" in the spec. The
    // value can be discarded when a connection is disconnected while
    // parsing an event.
    AtomicString m_id;
    // This variable corresponds to "last event ID string" in the spec.
    AtomicString m_lastEventId;

    Member<Client> m_client;
    OwnPtr<TextCodec> m_codec;

    bool m_isRecognizingCRLF = false;
    bool m_isRecognizingBOM = true;
    bool m_isStopped = false;
};

} // namespace blink

#endif // EventSourceParser_h
