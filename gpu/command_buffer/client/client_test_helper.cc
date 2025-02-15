// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for GLES2Implementation.

#include "gpu/command_buffer/client/client_test_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;

namespace gpu {

MockCommandBufferBase::MockCommandBufferBase() : put_offset_(0) {
}

MockCommandBufferBase::~MockCommandBufferBase() {
}

bool MockCommandBufferBase::Initialize() {
  return true;
}

CommandBuffer::State MockCommandBufferBase::GetLastState() {
  return state_;
}

int32_t MockCommandBufferBase::GetLastToken() {
  return state_.token;
}

void MockCommandBufferBase::SetGetOffset(int32_t get_offset) {
  state_.get_offset = get_offset;
}

void MockCommandBufferBase::WaitForTokenInRange(int32_t start, int32_t end) {}

void MockCommandBufferBase::WaitForGetOffsetInRange(int32_t start,
                                                    int32_t end) {
  state_.get_offset = put_offset_;
  OnFlush();
}

void MockCommandBufferBase::SetGetBuffer(int transfer_buffer_id) {
  ring_buffer_buffer_ = GetTransferBuffer(transfer_buffer_id);
  ring_buffer_ =
      static_cast<CommandBufferEntry*>(ring_buffer_buffer_->memory());
  state_.token = 10000;  // All token checks in the tests should pass.
}

// Get's the Id of the next transfer buffer that will be returned
// by CreateTransferBuffer. This is useful for testing expected ids.
int32_t MockCommandBufferBase::GetNextFreeTransferBufferId() {
  for (size_t ii = 0; ii < arraysize(transfer_buffer_buffers_); ++ii) {
    if (!transfer_buffer_buffers_[ii].get()) {
      return kTransferBufferBaseId + ii;
    }
  }
  return -1;
}

scoped_refptr<gpu::Buffer> MockCommandBufferBase::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  *id = GetNextFreeTransferBufferId();
  if (*id >= 0) {
    int32_t ndx = *id - kTransferBufferBaseId;
    std::unique_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
    shared_memory->CreateAndMapAnonymous(size);
    transfer_buffer_buffers_[ndx] =
        MakeBufferFromSharedMemory(std::move(shared_memory), size);
  }
  return GetTransferBuffer(*id);
}

void MockCommandBufferBase::DestroyTransferBufferHelper(int32_t id) {
  DCHECK_GE(id, kTransferBufferBaseId);
  DCHECK_LT(id, kTransferBufferBaseId + kMaxTransferBuffers);
  id -= kTransferBufferBaseId;
  transfer_buffer_buffers_[id] = NULL;
}

scoped_refptr<Buffer> MockCommandBufferBase::GetTransferBuffer(int32_t id) {
  DCHECK_GE(id, kTransferBufferBaseId);
  DCHECK_LT(id, kTransferBufferBaseId + kMaxTransferBuffers);
  return transfer_buffer_buffers_[id - kTransferBufferBaseId];
}

void MockCommandBufferBase::FlushHelper(int32_t put_offset) {
  put_offset_ = put_offset;
}

void MockCommandBufferBase::SetToken(int32_t token) {
  NOTREACHED();
  state_.token = token;
}

void MockCommandBufferBase::SetParseError(error::Error error) {
  NOTREACHED();
  state_.error = error;
}

void MockCommandBufferBase::SetContextLostReason(
    error::ContextLostReason reason) {
  NOTREACHED();
  state_.context_lost_reason = reason;
}

int32_t MockCommandBufferBase::GetPutOffset() {
  return put_offset_;
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32_t MockCommandBufferBase::kTransferBufferBaseId;
const int32_t MockCommandBufferBase::kMaxTransferBuffers;
#endif

MockClientCommandBuffer::MockClientCommandBuffer() {
  DelegateToFake();
}

MockClientCommandBuffer::~MockClientCommandBuffer() {
}

void MockClientCommandBuffer::Flush(int32_t put_offset) {
  FlushHelper(put_offset);
}

void MockClientCommandBuffer::OrderingBarrier(int32_t put_offset) {
  FlushHelper(put_offset);
}

void MockClientCommandBuffer::DelegateToFake() {
  ON_CALL(*this, DestroyTransferBuffer(_))
      .WillByDefault(Invoke(
          this, &MockCommandBufferBase::DestroyTransferBufferHelper));
}

MockClientCommandBufferMockFlush::MockClientCommandBufferMockFlush() {
  DelegateToFake();
}

MockClientCommandBufferMockFlush::~MockClientCommandBufferMockFlush() {
}

void MockClientCommandBufferMockFlush::DelegateToFake() {
  MockClientCommandBuffer::DelegateToFake();
  ON_CALL(*this, Flush(_))
      .WillByDefault(Invoke(
          this, &MockCommandBufferBase::FlushHelper));
}

MockClientGpuControl::MockClientGpuControl() {
}

MockClientGpuControl::~MockClientGpuControl() {
}

}  // namespace gpu


