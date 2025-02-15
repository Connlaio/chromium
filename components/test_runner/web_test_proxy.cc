// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "components/test_runner/accessibility_controller.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/text_input_controller.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_interfaces.h"

namespace test_runner {

WebTestProxyBase::WebTestProxyBase()
    : test_interfaces_(nullptr),
      delegate_(nullptr),
      web_view_(nullptr),
      web_widget_(nullptr),
      accessibility_controller_(new AccessibilityController(this)),
      event_sender_(new EventSender(this)),
      text_input_controller_(new TextInputController(this)) {}

WebTestProxyBase::~WebTestProxyBase() {
  test_interfaces_->WindowClosed(this);
}

void WebTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces_->WindowOpened(this);
}

void WebTestProxyBase::SetSendWheelGestures(bool send_gestures) {
  event_sender_->set_send_wheel_gestures(send_gestures);
}

void WebTestProxyBase::Reset() {
  accessibility_controller_->Reset();
  event_sender_->Reset();
  // text_input_controller_ doesn't have any state to reset.
}

void WebTestProxyBase::BindTo(blink::WebLocalFrame* frame) {
  accessibility_controller_->Install(frame);
  event_sender_->Install(frame);
  text_input_controller_->Install(frame);
}

void WebTestProxyBase::GetScreenOrientationForTesting(
    blink::WebScreenInfo& screen_info) {
  MockScreenOrientationClient* mock_client =
      test_interfaces_->GetTestRunner()->getMockScreenOrientationClient();
  if (mock_client->IsDisabled())
    return;
  // Override screen orientation information with mock data.
  screen_info.orientationType = mock_client->CurrentOrientationType();
  screen_info.orientationAngle = mock_client->CurrentOrientationAngle();
}

}  // namespace test_runner
