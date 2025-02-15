// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/interfaces/renderer.mojom.h"

namespace shell {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

// The default factory class for creating MojoRendererImpl.
class MojoRendererFactory : public RendererFactory {
 public:
  explicit MojoRendererFactory(
      shell::mojom::InterfaceProvider* interface_provider);
  ~MojoRendererFactory() final;

  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      const RequestSurfaceCB& request_surface_cb) final;

 private:
  shell::mojom::InterfaceProvider* interface_provider_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_
