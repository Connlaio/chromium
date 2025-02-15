// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_BINDING_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_BINDING_H_

#include <memory>

#include "base/macros.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class WindowServer;
class WindowTree;

// WindowTreeBinding manages the binding between a WindowTree and its mojo
// clients. WindowTreeBinding exists so that the client can be mocked for
// tests. WindowTree owns its associated WindowTreeBinding.
class WindowTreeBinding {
 public:
  explicit WindowTreeBinding(mojom::WindowTreeClient* client);
  virtual ~WindowTreeBinding();

  mojom::WindowTreeClient* client() { return client_; }

  // Obtains a new WindowManager. This should only be called once.
  virtual mojom::WindowManager* GetWindowManager() = 0;

  virtual void SetIncomingMethodCallProcessingPaused(bool paused) = 0;

 private:
  mojom::WindowTreeClient* client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeBinding);
};

// Bindings implementation of WindowTreeBinding.
class DefaultWindowTreeBinding : public WindowTreeBinding {
 public:
  DefaultWindowTreeBinding(WindowTree* tree,
                           WindowServer* window_server,
                           mojom::WindowTreeRequest service_request,
                           mojom::WindowTreeClientPtr client);
  DefaultWindowTreeBinding(WindowTree* tree,
                           WindowServer* window_server,
                           mojom::WindowTreeClientPtr client);
  ~DefaultWindowTreeBinding() override;

  // Use when created with the constructor that does not take a
  // WindowTreeRequest.
  mojom::WindowTreePtr CreateInterfacePtrAndBind();

  // WindowTreeBinding:
  mojom::WindowManager* GetWindowManager() override;
  void SetIncomingMethodCallProcessingPaused(bool paused) override;

 private:
  WindowServer* window_server_;
  mojo::Binding<mojom::WindowTree> binding_;
  mojom::WindowTreeClientPtr client_;
  mojom::WindowManagerAssociatedPtr window_manager_internal_;

  DISALLOW_COPY_AND_ASSIGN(DefaultWindowTreeBinding);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_BINDING_H_
