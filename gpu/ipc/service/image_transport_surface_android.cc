// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "base/logging.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/pass_through_image_transport_surface.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle surface_handle,
    gfx::GLSurface::Format format) {
  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  // On Android, the surface_handle is the id of the surface in the
  // GpuSurfaceTracker/GpuSurfaceLookup
  ANativeWindow* window =
      GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(surface_handle);
  if (!window) {
    LOG(WARNING) << "Failed to acquire native widget.";
    return nullptr;
  }
  scoped_refptr<gfx::GLSurface> surface =
      new gfx::NativeViewGLSurfaceEGL(window);
  bool initialize_success = surface->Initialize(format);
  ANativeWindow_release(window);
  if (!initialize_success)
    return scoped_refptr<gfx::GLSurface>();

  return scoped_refptr<gfx::GLSurface>(
      new PassThroughImageTransportSurface(manager, stub, surface.get()));
}

}  // namespace gpu
