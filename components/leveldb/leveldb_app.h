// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_APP_H_
#define COMPONENTS_LEVELDB_LEVELDB_APP_H_

#include "base/memory/scoped_ptr.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace leveldb {

class LevelDBApp : public shell::ShellClient,
                   public shell::InterfaceFactory<LevelDBService> {
 public:
  LevelDBApp();
  ~LevelDBApp() override;

 private:
  // |ShellClient| override:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // |InterfaceFactory<LevelDBService>| implementation:
  void Create(shell::Connection* connection,
              leveldb::LevelDBServiceRequest request) override;

  mojo::TracingImpl tracing_;
  scoped_ptr<LevelDBService> service_;
  mojo::BindingSet<LevelDBService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBApp);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_APP_H_
