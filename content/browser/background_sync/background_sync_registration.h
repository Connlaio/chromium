// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_

#include <stdint.h>

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/background_sync/background_sync_registration_options.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncRegistration {
 public:
  using RegistrationId = int64_t;

  static const RegistrationId kInitialId;

  BackgroundSyncRegistration() = default;
  BackgroundSyncRegistration(const BackgroundSyncRegistration& other) = default;
  BackgroundSyncRegistration& operator=(
      const BackgroundSyncRegistration& other) = default;
  ~BackgroundSyncRegistration() = default;

  bool Equals(const BackgroundSyncRegistration& other) const;
  bool IsValid() const;
  bool IsFiring() const;

  const BackgroundSyncRegistrationOptions* options() const { return &options_; }
  BackgroundSyncRegistrationOptions* options() { return &options_; }

  RegistrationId id() const { return id_; }
  void set_id(RegistrationId id) { id_ = id; }

  mojom::BackgroundSyncState sync_state() const { return sync_state_; }
  void set_sync_state(mojom::BackgroundSyncState state) { sync_state_ = state; }

  int num_attempts() const { return num_attempts_; }
  void set_num_attempts(int num_attempts) { num_attempts_ = num_attempts; }

  base::Time delay_until() const { return delay_until_; }
  void set_delay_until(base::Time delay_until) { delay_until_ = delay_until; }

 private:
  static const RegistrationId kInvalidRegistrationId;

  BackgroundSyncRegistrationOptions options_;
  RegistrationId id_ = kInvalidRegistrationId;
  mojom::BackgroundSyncState sync_state_ = mojom::BackgroundSyncState::PENDING;
  int num_attempts_ = 0;
  base::Time delay_until_;
};

}  // namespace content

namespace mojo {

template <>
struct CONTENT_EXPORT
    TypeConverter<std::unique_ptr<content::BackgroundSyncRegistration>,
                  content::mojom::SyncRegistrationPtr> {
  static std::unique_ptr<content::BackgroundSyncRegistration> Convert(
      const content::mojom::SyncRegistrationPtr& input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::mojom::SyncRegistrationPtr,
                                    content::BackgroundSyncRegistration> {
  static content::mojom::SyncRegistrationPtr Convert(
      const content::BackgroundSyncRegistration& input);
};

}  // namespace

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_
