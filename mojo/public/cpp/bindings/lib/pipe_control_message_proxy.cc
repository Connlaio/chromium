// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/pipe_control_message_proxy.h"

#include <stddef.h>
#include <utility>

#include "base/compiler_specific.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/interfaces/bindings/pipe_control_messages.mojom.h"

namespace mojo {
namespace internal {
namespace {

void SendRunOrClosePipeMessage(MessageReceiver* receiver,
                               pipe_control::RunOrClosePipeInputPtr input) {
  pipe_control::RunOrClosePipeMessageParamsPtr params_ptr(
      pipe_control::RunOrClosePipeMessageParams::New());
  params_ptr->input = std::move(input);

  size_t size = GetSerializedSize_(params_ptr, nullptr);
  MessageBuilder builder(pipe_control::kRunOrClosePipeMessageId, size);

  pipe_control::internal::RunOrClosePipeMessageParams_Data* params = nullptr;
  Serialize_(std::move(params_ptr), builder.buffer(), &params, nullptr);
  params->EncodePointers();
  builder.message()->set_interface_id(kInvalidInterfaceId);
  bool ok = receiver->Accept(builder.message());
  // This return value may be ignored as !ok implies the underlying message pipe
  // has encountered an error, which will be visible through other means.
  ALLOW_UNUSED_LOCAL(ok);
}

}  // namespace

PipeControlMessageProxy::PipeControlMessageProxy(MessageReceiver* receiver)
    : receiver_(receiver) {}

void PipeControlMessageProxy::NotifyPeerEndpointClosed(InterfaceId id) {
  pipe_control::PeerAssociatedEndpointClosedEventPtr event(
      pipe_control::PeerAssociatedEndpointClosedEvent::New());
  event->id = id;

  pipe_control::RunOrClosePipeInputPtr input(
      pipe_control::RunOrClosePipeInput::New());
  input->set_peer_associated_endpoint_closed_event(std::move(event));

  SendRunOrClosePipeMessage(receiver_, std::move(input));
}

void PipeControlMessageProxy::NotifyEndpointClosedBeforeSent(InterfaceId id) {
  pipe_control::AssociatedEndpointClosedBeforeSentEventPtr event(
      pipe_control::AssociatedEndpointClosedBeforeSentEvent::New());
  event->id = id;

  pipe_control::RunOrClosePipeInputPtr input(
      pipe_control::RunOrClosePipeInput::New());
  input->set_associated_endpoint_closed_before_sent_event(std::move(event));

  SendRunOrClosePipeMessage(receiver_, std::move(input));
}

}  // namespace internal
}  // namespace mojo
