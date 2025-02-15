// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "cc/blink/context_provider_web_context.h"
#include "gpu/command_buffer/client/context_support.h"
#include "third_party/WebKit/public/platform/functional/WebFunction.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<cc_blink::ContextProviderWebContext> provider)
    : provider_(provider) {
}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {}

blink::WebGraphicsContext3D* WebGraphicsContext3DProviderImpl::context3d() {
  return provider_->WebContext3D();
}

gpu::gles2::GLES2Interface* WebGraphicsContext3DProviderImpl::contextGL() {
  return provider_->ContextGL();
}

GrContext* WebGraphicsContext3DProviderImpl::grContext() {
  return provider_->GrContext();
}

gpu::Capabilities WebGraphicsContext3DProviderImpl::getCapabilities() {
  return provider_->ContextCapabilities();
}

void WebGraphicsContext3DProviderImpl::setLostContextCallback(
    blink::WebClosure c) {
  provider_->SetLostContextCallback(c.TakeBaseCallback());
}

void WebGraphicsContext3DProviderImpl::setErrorMessageCallback(
    blink::WebFunction<void(const char*, int32_t)> c) {
  provider_->ContextSupport()->SetErrorMessageCallback(c.TakeBaseCallback());
}

}  // namespace content
