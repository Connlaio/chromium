// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"

using ::base::SharedMemory;

namespace gpu {

CommandBufferService::CommandBufferService(
    TransferBufferManagerInterface* transfer_buffer_manager)
    : ring_buffer_id_(-1),
      shared_state_(NULL),
      num_entries_(0),
      get_offset_(0),
      put_offset_(0),
      transfer_buffer_manager_(transfer_buffer_manager),
      token_(0),
      generation_(0),
      error_(error::kNoError),
      context_lost_reason_(error::kUnknown) {
}

CommandBufferService::~CommandBufferService() {
}

bool CommandBufferService::Initialize() {
  return true;
}

CommandBufferService::State CommandBufferService::GetLastState() {
  State state;
  state.get_offset = get_offset_;
  state.token = token_;
  state.error = error_;
  state.context_lost_reason = context_lost_reason_;
  state.generation = ++generation_;

  return state;
}

int32_t CommandBufferService::GetLastToken() {
  return GetLastState().token;
}

void CommandBufferService::UpdateState() {
  if (shared_state_) {
    CommandBufferService::State state = GetLastState();
    shared_state_->Write(state);
  }
}

void CommandBufferService::WaitForTokenInRange(int32_t start, int32_t end) {
  DCHECK(error_ != error::kNoError || InRange(start, end, token_));
}

void CommandBufferService::WaitForGetOffsetInRange(int32_t start, int32_t end) {
  DCHECK(error_ != error::kNoError || InRange(start, end, get_offset_));
}

void CommandBufferService::Flush(int32_t put_offset) {
  if (put_offset < 0 || put_offset > num_entries_) {
    error_ = gpu::error::kOutOfBounds;
    return;
  }

  put_offset_ = put_offset;

  if (!put_offset_change_callback_.is_null())
    put_offset_change_callback_.Run();
}

void CommandBufferService::OrderingBarrier(int32_t put_offset) {
  Flush(put_offset);
}

void CommandBufferService::SetGetBuffer(int32_t transfer_buffer_id) {
  DCHECK_EQ(-1, ring_buffer_id_);
  DCHECK_EQ(put_offset_, get_offset_);  // Only if it's empty.
  // If the buffer is invalid we handle it gracefully.
  // This means ring_buffer_ can be NULL.
  ring_buffer_ = GetTransferBuffer(transfer_buffer_id);
  ring_buffer_id_ = transfer_buffer_id;
  int32_t size = ring_buffer_.get() ? ring_buffer_->size() : 0;
  num_entries_ = size / sizeof(CommandBufferEntry);
  put_offset_ = 0;
  SetGetOffset(0);
  if (!get_buffer_change_callback_.is_null()) {
    get_buffer_change_callback_.Run(ring_buffer_id_);
  }

  UpdateState();
}

void CommandBufferService::SetSharedStateBuffer(
    std::unique_ptr<BufferBacking> shared_state_buffer) {
  shared_state_buffer_ = std::move(shared_state_buffer);
  DCHECK(shared_state_buffer_->GetSize() >= sizeof(*shared_state_));

  shared_state_ =
      static_cast<CommandBufferSharedState*>(shared_state_buffer_->GetMemory());

  UpdateState();
}

void CommandBufferService::SetGetOffset(int32_t get_offset) {
  DCHECK(get_offset >= 0 && get_offset < num_entries_);
  get_offset_ = get_offset;
}

scoped_refptr<Buffer> CommandBufferService::CreateTransferBuffer(size_t size,
                                                                 int32_t* id) {
  *id = -1;

  std::unique_ptr<SharedMemory> shared_memory(new SharedMemory());
  if (!shared_memory->CreateAndMapAnonymous(size)) {
    if (error_ == error::kNoError)
      error_ = gpu::error::kOutOfBounds;
    return NULL;
  }

  static int32_t next_id = 1;
  *id = next_id++;

  if (!RegisterTransferBuffer(
          *id, MakeBackingFromSharedMemory(std::move(shared_memory), size))) {
    if (error_ == error::kNoError)
      error_ = gpu::error::kOutOfBounds;
    *id = -1;
    return NULL;
  }

  return GetTransferBuffer(*id);
}

void CommandBufferService::DestroyTransferBuffer(int32_t id) {
  transfer_buffer_manager_->DestroyTransferBuffer(id);
  if (id == ring_buffer_id_) {
    ring_buffer_id_ = -1;
    ring_buffer_ = NULL;
    num_entries_ = 0;
    get_offset_ = 0;
    put_offset_ = 0;
  }
}

scoped_refptr<Buffer> CommandBufferService::GetTransferBuffer(int32_t id) {
  return transfer_buffer_manager_->GetTransferBuffer(id);
}

bool CommandBufferService::RegisterTransferBuffer(
    int32_t id,
    std::unique_ptr<BufferBacking> buffer) {
  return transfer_buffer_manager_->RegisterTransferBuffer(id,
                                                          std::move(buffer));
}

void CommandBufferService::SetToken(int32_t token) {
  token_ = token;
  UpdateState();
}

void CommandBufferService::SetParseError(error::Error error) {
  if (error_ == error::kNoError) {
    error_ = error;
    if (!parse_error_callback_.is_null())
      parse_error_callback_.Run();
  }
}

void CommandBufferService::SetContextLostReason(
    error::ContextLostReason reason) {
  context_lost_reason_ = reason;
}

int32_t CommandBufferService::GetPutOffset() {
  return put_offset_;
}

void CommandBufferService::SetPutOffsetChangeCallback(
    const base::Closure& callback) {
  put_offset_change_callback_ = callback;
}

void CommandBufferService::SetGetBufferChangeCallback(
    const GetBufferChangedCallback& callback) {
  get_buffer_change_callback_ = callback;
}

void CommandBufferService::SetParseErrorCallback(
    const base::Closure& callback) {
  parse_error_callback_ = callback;
}

}  // namespace gpu
