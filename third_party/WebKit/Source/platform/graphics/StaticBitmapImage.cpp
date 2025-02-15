// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(PassRefPtr<SkImage> image)
{
    if (!image)
        return nullptr;
    return adoptRef(new StaticBitmapImage(image));
}

StaticBitmapImage::StaticBitmapImage(PassRefPtr<SkImage> image) : m_image(image)
{
    ASSERT(m_image);
}

StaticBitmapImage::~StaticBitmapImage() { }

IntSize StaticBitmapImage::size() const
{
    return IntSize(m_image->width(), m_image->height());
}

bool StaticBitmapImage::currentFrameKnownToBeOpaque(MetadataMode)
{
    return m_image->isOpaque();
}

void StaticBitmapImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect,
    const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode clampMode)
{
    // Note: Sizes < 0 should never happen, except that the layout arithmetic
    // may overflow in degenerate use cases, so we need to check for negatives,
    // rather than only handle the isEmpty() case. See layout test
    // fast/canvas/bug544329.html
    if (dstRect.width() <= 0 || dstRect.height() <= 0 || srcRect.width() <= 0 || srcRect.height() <= 0)
        return;

    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.intersect(FloatRect(0, 0, m_image->width(), m_image->height()));

    if (adjustedSrcRect.isEmpty())
        return; // Nothing to draw.

    ASSERT(adjustedSrcRect.width() <= m_image->width() && adjustedSrcRect.height() <= m_image->height());

    SkRect srcSkRect = adjustedSrcRect;
    canvas->drawImageRect(m_image.get(), srcSkRect, dstRect, &paint,
        WebCoreClampingModeToSkiaRectConstraint(clampMode));

    if (ImageObserver* observer = getImageObserver())
        observer->didDraw(this);
}

PassRefPtr<SkImage> StaticBitmapImage::imageForCurrentFrame()
{
    return m_image;
}

} // namespace blink
