// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_

#include <EGL/egl.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
class CommandBufferService;
class GpuControl;
class CommandExecutor;
class TransferBuffer;
class TransferBufferManagerInterface;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace egl {

class Config;
class Surface;

class Display : private gpu::GpuControl {
 public:
  explicit Display(EGLNativeDisplayType display_id);
  ~Display() override;

  void SetCreateOffscreen(int width, int height) {
    create_offscreen_ = true;
    create_offscreen_width_ = width;
    create_offscreen_height_ = height;
  }

  bool is_initialized() const { return is_initialized_; }
  bool Initialize();

  // Config routines.
  bool IsValidConfig(EGLConfig config);
  bool ChooseConfigs(
      EGLConfig* configs, EGLint config_size, EGLint* num_config);
  bool GetConfigs(EGLConfig* configs, EGLint config_size, EGLint* num_config);
  bool GetConfigAttrib(EGLConfig config, EGLint attribute, EGLint* value);

  // Surface routines.
  bool IsValidNativeWindow(EGLNativeWindowType win);
  bool IsValidSurface(EGLSurface surface);
  EGLSurface CreateWindowSurface(EGLConfig config,
                                 EGLNativeWindowType win,
                                 const EGLint* attrib_list);
  void DestroySurface(EGLSurface surface);
  void SwapBuffers(EGLSurface surface);

  // Context routines.
  bool IsValidContext(EGLContext ctx);
  EGLContext CreateContext(EGLConfig config,
                           EGLContext share_ctx,
                           const EGLint* attrib_list);
  void DestroyContext(EGLContext ctx);
  bool MakeCurrent(EGLSurface draw, EGLSurface read, EGLContext ctx);

  // GpuControl implementation.
  void SetGpuControlClient(gpu::GpuControlClient*) override;
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internalformat,
                                     unsigned usage) override;
  void SignalQuery(uint32_t query, const base::Closure& callback) override;
  void SetLock(base::Lock*) override;
  bool IsGpuChannelLost() override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  int32_t GetExtraCommandBufferData() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) override;

 private:
  EGLNativeDisplayType display_id_;

  gpu::GpuPreferences gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  bool is_initialized_;

  bool create_offscreen_;
  int create_offscreen_width_;
  int create_offscreen_height_;
  uint64_t next_fence_sync_release_;

  scoped_refptr<gpu::TransferBufferManagerInterface> transfer_buffer_manager_;
  std::unique_ptr<gpu::CommandBufferService> command_buffer_;
  std::unique_ptr<gpu::CommandExecutor> executor_;
  std::unique_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_refptr<gfx::GLContext> gl_context_;
  scoped_refptr<gfx::GLSurface> gl_surface_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_cmd_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;

  // TODO(alokp): Support more than one config, surface, and context.
  std::unique_ptr<Config> config_;
  std::unique_ptr<Surface> surface_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> context_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace egl

#endif  // GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_
