// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/resource_provider/resource_provider_app.h"

#include <utility>

#include "components/resource_provider/file_utils.h"
#include "components/resource_provider/resource_provider_impl.h"
#include "services/shell/public/cpp/connection.h"
#include "url/gurl.h"

namespace resource_provider {

ResourceProviderApp::ResourceProviderApp(
    const std::string& resource_provider_app_url)
    : resource_provider_app_url_(resource_provider_app_url) {
}

ResourceProviderApp::~ResourceProviderApp() {
}

void ResourceProviderApp::Initialize(shell::Connector* connector,
                                     const shell::Identity& identity,
                                     uint32_t id) {
  tracing_.Initialize(connector, identity.name());
}

bool ResourceProviderApp::AcceptConnection(shell::Connection* connection) {
  const base::FilePath app_path(
      GetPathForApplicationName(connection->GetRemoteIdentity().name()));
  if (app_path.empty())
    return false;  // The specified app has no resources.

  connection->AddInterface<ResourceProvider>(this);
  return true;
}

void ResourceProviderApp::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<ResourceProvider> request) {
  const base::FilePath app_path(
      GetPathForApplicationName(connection->GetRemoteIdentity().name()));
  // We validated path at AcceptConnection() time, so it should still
  // be valid.
  CHECK(!app_path.empty());
  bindings_.AddBinding(
      new ResourceProviderImpl(app_path, resource_provider_app_url_),
      std::move(request));
}

}  // namespace resource_provider
