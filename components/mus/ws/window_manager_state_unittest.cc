// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_state.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/mus/public/cpp/event_matcher_util.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/accelerator.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_init_params.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace test {

class WindowManagerStateTest : public testing::Test {
 public:
  WindowManagerStateTest();
  ~WindowManagerStateTest() override {}

  std::unique_ptr<Accelerator> CreateAccelerator();

  // Creates a child |server_window| with associataed |window_tree| and
  // |test_client|. The window is setup for processing input.
  void CreateSecondaryTree(TestWindowTreeClient** test_client,
                           WindowTree** window_tree,
                           ServerWindow** server_window);

  void DispatchInputEventToWindow(ServerWindow* target,
                                  bool in_nonclient_area,
                                  const ui::Event& event,
                                  Accelerator* accelerator);
  void OnEventAckTimeout();

  WindowTree* tree() { return tree_; }
  ServerWindow* window() { return window_; }
  TestWindowManager* window_manager() { return &window_manager_; }
  TestWindowTreeClient* wm_client() { return wm_client_; }
  WindowManagerState* window_manager_state() { return window_manager_state_; }

  // testing::Test:
  void SetUp() override;

 private:
  int32_t cursor_id_;

  // Owned by WindowServer's DisplayManager.
  Display* display_;
  // Owned by Display.
  WindowManagerState* window_manager_state_;
  // Owned by WindowServer.
  TestDisplayBinding* display_binding_;
  // Destroyed in TearDown.
  WindowTree* tree_;
  // Owned by Display.
  WindowTree* original_tree_;
  // TestWindowTreeClient that is used for the WM connection. Owned by
  // |window_server_delegate_|
  TestWindowTreeClient* wm_client_;

  TestWindowServerDelegate window_server_delegate_;
  std::unique_ptr<WindowServer> window_server_;
  // Handles WindowStateManager ack timeouts.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestWindowManager window_manager_;
  ServerWindow* window_;
  // Needed to Bind to |wm_client_|
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerStateTest);
};

WindowManagerStateTest::WindowManagerStateTest()
    : cursor_id_(0),
      task_runner_(new base::TestSimpleTaskRunner),
      platform_display_factory_(&cursor_id_) {}

std::unique_ptr<Accelerator> WindowManagerStateTest::CreateAccelerator() {
  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      mus::mojom::KeyboardCode::W, mus::mojom::kEventFlagControlDown);
  matcher->accelerator_phase = mojom::AcceleratorPhase::POST_TARGET;
  uint32_t accelerator_id = 1;
  std::unique_ptr<Accelerator> accelerator(
      new Accelerator(accelerator_id, *matcher));
  return accelerator;
}

void WindowManagerStateTest::CreateSecondaryTree(
    TestWindowTreeClient** test_client,
    WindowTree** window_tree,
    ServerWindow** server_window) {
  WindowTree* tree1 = window_server_->GetTreeWithRoot(window_);
  ASSERT_TRUE(tree1 != nullptr);
  ASSERT_NE(tree1, tree());

  const ClientWindowId child1_id(
      WindowIdToTransportId(WindowId(tree1->id(), 1)));
  EXPECT_TRUE(tree1->NewWindow(child1_id, ServerWindow::Properties()));
  ServerWindow* child1 = tree1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  ClientWindowId window_id;
  tree1->IsWindowKnown(window_, &window_id);
  EXPECT_TRUE(tree1->AddWindow(window_id, child1_id));
  tree1->GetDisplay(window_)->AddActivationParent(window_);

  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));

  TestWindowTreeClient* embed_connection =
      window_server_delegate_.last_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  *test_client = embed_connection;
  *window_tree = tree1;
  *server_window = child1;
}

void WindowManagerStateTest::DispatchInputEventToWindow(
    ServerWindow* target,
    bool in_nonclient_area,
    const ui::Event& event,
    Accelerator* accelerator) {
  WindowManagerStateTestApi test_api(window_manager_state_);
  test_api.DispatchInputEventToWindow(target, in_nonclient_area, event,
                                      accelerator);
}

void WindowManagerStateTest::OnEventAckTimeout() {
  WindowManagerStateTestApi test_api(window_manager_state_);
  test_api.OnEventAckTimeout();
}

void WindowManagerStateTest::SetUp() {
  message_loop_.SetTaskRunner(task_runner_);

  PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
  window_server_.reset(new WindowServer(&window_server_delegate_,
                                        scoped_refptr<SurfacesState>()));

  PlatformDisplayInitParams display_init_params;
  display_ = new Display(window_server_.get(), display_init_params);
  display_binding_ = new TestDisplayBinding(display_, window_server_.get());
  display_->Init(base::WrapUnique(display_binding_));

  wm_client_ = window_server_delegate_.last_client();
  window_manager_state_ = display_->GetActiveWindowManagerState();

  tree_ = window_server_->GetTreeWithId(1);
  const ClientWindowId embed_window_id(WindowIdToTransportId(WindowId(1, 1)));
  EXPECT_TRUE(tree_->NewWindow(embed_window_id, ServerWindow::Properties()));
  ClientWindowId root_id;
  if (tree_->roots().size() == 1u) {
    // If window isn't known we'll return 0, which should then error out.
    tree_->IsWindowKnown(*(tree_->roots().begin()), &root_id);
  }

  EXPECT_TRUE(tree_->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(tree_->AddWindow(root_id, embed_window_id));
  display_->root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = GetProxy(&client);
  wm_client_->Bind(std::move(client_request));
  tree_->Embed(embed_window_id, std::move(client));
  window_ = tree_->GetWindowByClientId(embed_window_id);
  window_->SetBounds(gfx::Rect(0, 0, 50, 50));

  WindowTreeTestApi(tree_).set_window_manager_internal(&window_manager_);
  wm_client_->tracker()->changes()->clear();
}

// Tests that when an event is dispatched with no accelerator, that post target
// accelerator is not triggered.
TEST_F(WindowManagerStateTest, NullAccelerator) {
  WindowManagerState* state = window_manager_state();
  EXPECT_TRUE(state);

  ServerWindow* target = window();
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  DispatchInputEventToWindow(target, true, key, nullptr);
  WindowTree* target_tree = tree();
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  state->OnEventAck(target_tree, mojom::EventResult::UNHANDLED);
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that when a post target accelerator is provided on an event, that it is
// called on ack.
TEST_F(WindowManagerStateTest, PostTargetAccelerator) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, true, key, accelerator.get());
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  window_manager_state()->OnEventAck(tree(), mojom::EventResult::UNHANDLED);
  EXPECT_TRUE(window_manager()->on_accelerator_called());
  EXPECT_EQ(accelerator->id(), window_manager()->on_accelerator_id());
}

// Tests that when a client handles an event that post target accelerators are
// not called.
TEST_F(WindowManagerStateTest, ClientHandlesEvent) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, true, key, accelerator.get());
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  window_manager_state()->OnEventAck(tree(), mojom::EventResult::HANDLED);
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that when an accelerator is deleted before an ack, that it is not
// called.
TEST_F(WindowManagerStateTest, AcceleratorDeleted) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator(CreateAccelerator());

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, true, key, accelerator.get());
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  accelerator.reset();
  window_manager_state()->OnEventAck(tree(), mojom::EventResult::UNHANDLED);
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that a events arriving before an ack don't notify the tree until the
// ack arrives, and that the correct accelerator is called.
TEST_F(WindowManagerStateTest, EnqueuedAccelerators) {
  WindowManagerState* state = window_manager_state();

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator(CreateAccelerator());

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, true, key, accelerator.get());
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  tracker->changes()->clear();
  ui::KeyEvent key2(ui::ET_KEY_PRESSED, ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      mus::mojom::KeyboardCode::Y, mus::mojom::kEventFlagControlDown);
  matcher->accelerator_phase = mojom::AcceleratorPhase::POST_TARGET;
  uint32_t accelerator_id = 2;
  std::unique_ptr<Accelerator> accelerator2(
      new Accelerator(accelerator_id, *matcher));
  DispatchInputEventToWindow(target, true, key2, accelerator2.get());
  EXPECT_TRUE(tracker->changes()->empty());

  WindowTree* target_tree = tree();
  WindowTreeTestApi(target_tree).ClearAck();
  state->OnEventAck(target_tree, mojom::EventResult::UNHANDLED);
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);
  EXPECT_TRUE(window_manager()->on_accelerator_called());
  EXPECT_EQ(accelerator->id(), window_manager()->on_accelerator_id());
}

// Tests that the accelerator is not sent when the tree is dying.
TEST_F(WindowManagerStateTest, DeleteTree) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, true, key, accelerator.get());
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  window_manager_state()->OnWillDestroyTree(tree());
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that if a tree is destroyed before acking, that the accelerator is
// still sent if it is not the root tree.
TEST_F(WindowManagerStateTest, DeleteNonRootTree) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* target_tree = nullptr;
  ServerWindow* target = nullptr;
  CreateSecondaryTree(&embed_connection, &target_tree, &target);
  TestWindowManager target_window_manager;
  WindowTreeTestApi(target_tree)
      .set_window_manager_internal(&target_window_manager);

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();
  DispatchInputEventToWindow(target, true, key, accelerator.get());
  TestChangeTracker* tracker = embed_connection->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=2,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);
  EXPECT_TRUE(wm_client()->tracker()->changes()->empty());

  window_manager_state()->OnWillDestroyTree(target_tree);
  EXPECT_FALSE(target_window_manager.on_accelerator_called());
  EXPECT_TRUE(window_manager()->on_accelerator_called());
}

// Tests that when an ack times out that the accelerator is notified.
TEST_F(WindowManagerStateTest, AckTimeout) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();
  DispatchInputEventToWindow(window(), true, key, accelerator.get());
  TestChangeTracker* tracker = wm_client()->tracker();
  EXPECT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=1",
            ChangesToDescription1(*tracker->changes())[0]);

  OnEventAckTimeout();
  EXPECT_TRUE(window_manager()->on_accelerator_called());
  EXPECT_EQ(accelerator->id(), window_manager()->on_accelerator_id());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
