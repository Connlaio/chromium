/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JavaScriptCallFrame_h
#define JavaScriptCallFrame_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include <v8.h>

namespace blink {

class JavaScriptCallFrame {
public:
    static PassOwnPtr<JavaScriptCallFrame> create(v8::Local<v8::Context> debuggerContext, v8::Local<v8::Object> callFrame)
    {
        return adoptPtr(new JavaScriptCallFrame(debuggerContext, callFrame));
    }
    ~JavaScriptCallFrame();

    int sourceID() const;
    int line() const;
    int column() const;
    int contextId() const;

    bool isAtReturn() const;
    v8::Local<v8::Object> details() const;

    v8::MaybeLocal<v8::Value> evaluate(v8::Local<v8::Value> expression);
    v8::MaybeLocal<v8::Value> restart();
    v8::MaybeLocal<v8::Value> setVariableValue(int scopeNumber, v8::Local<v8::Value> variableName, v8::Local<v8::Value> newValue);
private:
    JavaScriptCallFrame(v8::Local<v8::Context> debuggerContext, v8::Local<v8::Object> callFrame);

    int callV8FunctionReturnInt(const char* name) const;

    v8::Isolate* m_isolate;
    OwnPtr<JavaScriptCallFrame> m_caller;
    v8::Global<v8::Context> m_debuggerContext;
    v8::Global<v8::Object> m_callFrame;
    v8::Global<v8::FunctionTemplate> m_wrapperTemplate;
};

using JavaScriptCallFrames = protocol::Vector<OwnPtr<JavaScriptCallFrame>>;

} // namespace blink

#endif // JavaScriptCallFrame_h
