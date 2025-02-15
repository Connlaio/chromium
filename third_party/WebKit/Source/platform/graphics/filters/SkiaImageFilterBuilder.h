/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SkiaImageFilterBuilder_h
#define SkiaImageFilterBuilder_h

#include "platform/PlatformExport.h"
#include "platform/graphics/ColorSpace.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImageFilter;

namespace blink {

class AffineTransform;
class BoxReflection;
class CompositorFilterOperations;
class FilterEffect;
class FilterOperations;
class Image;

namespace SkiaImageFilterBuilder {

PLATFORM_EXPORT sk_sp<SkImageFilter> build(FilterEffect*, ColorSpace, bool requiresPMColorValidation = true);
PLATFORM_EXPORT void buildFilterOperations(const FilterOperations&, CompositorFilterOperations*);
PLATFORM_EXPORT sk_sp<SkImageFilter> buildTransform(const AffineTransform&, sk_sp<SkImageFilter> input);

PLATFORM_EXPORT sk_sp<SkImageFilter> transformColorSpace(
    sk_sp<SkImageFilter> input, ColorSpace srcColorSpace, ColorSpace dstColorSpace);

PLATFORM_EXPORT sk_sp<SkImageFilter> buildBoxReflectFilter(const BoxReflection&, sk_sp<SkImageFilter> input);

} // namespace SkiaImageFilterBuilder
} // namespace blink

#endif
