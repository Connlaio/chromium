// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TEST_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TEST_H_

#include <memory>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"

class Browser;
class FullscreenController;
class FullscreenNotificationObserver;

// Utility definition for mapping enum values to strings in switch statements.
#define ENUM_TO_STRING(enum) case enum: return #enum

// Test fixture used to test Fullscreen Controller through exhaustive sequences
// of events in unit and interactive tests.
//
// Because operating system window managers are too unreliable (they result in
// flakiness at around 1 out of 1000 runs) this fixture is designed to be run
// on testing infrastructure in unit tests mocking out the platforms' behavior.
// To verify that behavior interactive tests exist but are left disabled and
// only run manually when verifying the consistency of the
// FullscreenControllerTestWindow.
class FullscreenControllerStateTest {
 public:
  // Events names for FullscreenController methods.
  enum Event {
    TOGGLE_FULLSCREEN,         // ToggleBrowserFullscreenMode()
    TOGGLE_FULLSCREEN_CHROME,  // ToggleBrowserFullscreenWithToolbar()
    TAB_FULLSCREEN_TRUE,       // ToggleFullscreenModeForTab(, true)
    TAB_FULLSCREEN_FALSE,      // ToggleFullscreenModeForTab(, false)
    BUBBLE_EXIT_LINK,          // ExitTabOrBrowserFullscreenToPreviousState()
    BUBBLE_ALLOW,              // OnAcceptFullscreenPermission()
    BUBBLE_DENY,               // OnDenyFullscreenPermission()
    WINDOW_CHANGE,             // ChangeWindowFullscreenState()
    NUM_EVENTS,
    EVENT_INVALID,
  };

  // Conceptual states of the Fullscreen Controller, these do not correspond
  // to particular implemenation details.
  enum State {
    // The window is not in fullscreen.
    STATE_NORMAL,
    // User-initiated fullscreen.
    STATE_BROWSER_FULLSCREEN_NO_CHROME,
    // Mac User-initiated 'Lion Fullscreen' with browser chrome. OSX 10.7+ only.
    STATE_BROWSER_FULLSCREEN_WITH_CHROME,
    // HTML5 tab-initiated fullscreen.
    STATE_TAB_FULLSCREEN,
    // Both tab and browser fullscreen.
    STATE_TAB_BROWSER_FULLSCREEN,
    // Both tab and browser fullscreen, displayed without chrome, but exits tab
    // fullscreen to STATE_BROWSER_FULLSCREEN_WITH_CHROME.
    STATE_TAB_BROWSER_FULLSCREEN_CHROME,
    // TO_ states are asynchronous states waiting for window state change
    // before transitioning to their named state.
    STATE_TO_NORMAL,
    STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,
    STATE_TO_BROWSER_FULLSCREEN_WITH_CHROME,
    STATE_TO_TAB_FULLSCREEN,
    NUM_STATES,
    STATE_INVALID,
  };

  static const int kMaxStateNameLength = 39;

  FullscreenControllerStateTest();
  virtual ~FullscreenControllerStateTest();

  static const char* GetStateString(State state);
  static const char* GetEventString(Event event);

  // Returns true if FullscreenController::WindowFullscreenStateChanged()
  // will be called and re-enter FullscreenController before
  // FullscreenController methods complete.
  static bool IsWindowFullscreenStateChangedReentrant();

  // Returns true if |state| can be persistent. This is true for all of the
  // states without "_TO_" in their name.
  static bool IsPersistentState(State state);

  // Causes Fullscreen Controller to transition to an arbitrary state.
  void TransitionToState(State state);

  // Makes one state change to approach |destination_state| via shortest path.
  // Returns true if a state change is made.
  // Repeated calls are needed to reach the destination.
  bool TransitionAStepTowardState(State destination_state);

  // Calls FullscreenController::ChangeWindowFullscreenState if needed because
  // a mock BrowserWindow is being used.
  virtual void ChangeWindowFullscreenState() {}

  // Returns a description of the window's state, may return NULL.
  // FullscreenControllerStateTest owns the returned pointer.
  virtual const char* GetWindowStateString();

  // Causes the |event| to occur and return true on success.
  virtual bool InvokeEvent(Event event);

  // Checks that window state matches the expected controller state.
  virtual void VerifyWindowState();

  // Wait for NOTIFICATION_FULLSCREEN_CHANGED if a notification should have been
  // sent in transitioning to |state_| from the previous persistent state.
  void MaybeWaitForNotification();

  // Tests all states with all permutations of multiple events to detect
  // lingering state issues that would bleed over to other states.
  // I.E. for each state test all combinations of events E1, E2, E3.
  //
  // This produces coverage for event sequences that may happen normally but
  // would not be exposed by traversing to each state via TransitionToState().
  // TransitionToState() always takes the same path even when multiple paths
  // exist.
  void TestTransitionsForEachState();

  // Log transition_table_ to a string for debugging.
  std::string GetTransitionTableAsString() const;
  // Log state_transitions_ to a string for debugging.
  std::string GetStateTransitionsAsString() const;

 protected:
  // Set of enumerations (created with a helper macro) for _FALSE, _TRUE, and
  // _NO_EXPECTATION values to be passed to VerifyWindowStateExpectations().
  #define EXPECTATION_ENUM(enum_name, enum_prefix) \
      enum enum_name { \
        enum_prefix##_FALSE, \
        enum_prefix##_TRUE, \
        enum_prefix##_NO_EXPECTATION \
      }
  EXPECTATION_ENUM(FullscreenWithToolbarExpectation, FULLSCREEN_WITH_CHROME);
  EXPECTATION_ENUM(FullscreenForBrowserExpectation, FULLSCREEN_FOR_BROWSER);
  EXPECTATION_ENUM(FullscreenForTabExpectation, FULLSCREEN_FOR_TAB);

  // Generated information about the transitions between states.
  struct StateTransitionInfo {
    StateTransitionInfo()
        : event(EVENT_INVALID),
          state(STATE_INVALID),
          distance(NUM_STATES) {}
    Event event;  // The |Event| that will cause the state transition.
    State state;  // The adjacent |State| transitioned to; not the final state.
    int distance;  // Steps to final state. NUM_STATES represents unknown.
  };

  // Returns next transition info for shortest path from source to destination.
  StateTransitionInfo NextTransitionInShortestPath(State source,
                                                   State destination,
                                                   int search_limit);

  // Returns a detailed log of what FullscreenControllerStateTest has done
  // up to this point, to be reported when tests fail.
  std::string GetAndClearDebugLog();

  // Returns true if the |state| & |event| pair should be skipped.
  virtual bool ShouldSkipStateAndEventPair(State state, Event event);

  // Returns true if a test should be skipped entirely, e.g. due to platform.
  virtual bool ShouldSkipTest(State state, Event event);

  // Runs one test of transitioning to a state and invoking an event.
  virtual void TestStateAndEvent(State state, Event event);

  // Checks that window state matches the expected controller state.
  virtual void VerifyWindowStateExpectations(
      FullscreenWithToolbarExpectation fullscreen_with_toolbar,
      FullscreenForBrowserExpectation fullscreen_for_browser,
      FullscreenForTabExpectation fullscreen_for_tab);

  virtual Browser* GetBrowser() = 0;
  FullscreenController* GetFullscreenController();

  // The state the FullscreenController is expected to be in.
  State state() const { return state_; }

 private:
  // The state the FullscreenController is expected to be in.
  State state_;

  // The state when the previous NOTIFICATION_FULLSCREEN_CHANGED notification
  // was received.
  State last_notification_received_state_;

  // Listens for the NOTIFICATION_FULLSCREEN_CHANGED notification.
  std::unique_ptr<FullscreenNotificationObserver>
      fullscreen_notification_observer_;

  // Human defined |State| that results given each [state][event] pair.
  State transition_table_[NUM_STATES][NUM_EVENTS];

  // Generated information about the transitions between states [from][to].
  // View generated data with: out/Release/unit_tests
  //     --gtest_filter="FullscreenController*DebugLogStateTables"
  //     --gtest_also_run_disabled_tests
  StateTransitionInfo state_transitions_[NUM_STATES][NUM_STATES];

  // Log of operations reported on errors via GetAndClearDebugLog().
  std::ostringstream debugging_log_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerStateTest);
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_STATE_TEST_H_
