// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_service.h"

#include <stddef.h>

#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "components/policy/core/common/remote_commands/test_remote_command_job.h"
#include "components/policy/core/common/remote_commands/testing_remote_commands_server.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ReturnNew;

namespace policy {

namespace {

namespace em = enterprise_management;

const char kDMToken[] = "dmtoken";
const char kTestPayload[] = "_testing_payload_";
const int kTestCommandExecutionTimeInSeconds = 1;
const int kTestClientServerCommunicationDelayInSeconds = 3;

void ExpectSucceededJob(const std::string& expected_payload,
                        const em::RemoteCommandResult& command_result) {
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            command_result.result());
  EXPECT_EQ(expected_payload, command_result.payload());
}

}  // namespace

// Mocked RemoteCommand factory to allow us to build test commands.
class MockTestRemoteCommandFactory : public RemoteCommandsFactory {
 public:
  MockTestRemoteCommandFactory() {
    ON_CALL(*this, BuildTestCommand())
        .WillByDefault(ReturnNew<TestRemoteCommandJob>(
            true,
            base::TimeDelta::FromSeconds(kTestCommandExecutionTimeInSeconds)));
  }

  MOCK_METHOD0(BuildTestCommand, TestRemoteCommandJob*());

 private:
  // RemoteCommandJobsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      em::RemoteCommand_Type type) override {
    if (type != em::RemoteCommand_Type_COMMAND_ECHO_TEST) {
      ADD_FAILURE();
      return nullptr;
    }
    return base::WrapUnique<RemoteCommandJob>(BuildTestCommand());
  }

  DISALLOW_COPY_AND_ASSIGN(MockTestRemoteCommandFactory);
};

// A mocked CloudPolicyClient to interact with a TestingRemoteCommandsServer.
class TestingCloudPolicyClientForRemoteCommands : public CloudPolicyClient {
 public:
  explicit TestingCloudPolicyClientForRemoteCommands(
      TestingRemoteCommandsServer* server)
      : CloudPolicyClient(std::string(), /* machine_id */
                          std::string(), /* machine_model */
                          std::string(), /* verification_key_hash */
                          nullptr,
                          nullptr),
        server_(server) {
    dm_token_ = kDMToken;
  }

  ~TestingCloudPolicyClientForRemoteCommands() override {
    EXPECT_TRUE(expected_fetch_commands_calls_.empty());
  }

  // Expect a FetchRemoteCommands() call with |expected_command_results|
  // commands results sent and |expected_fetched_commands| commands fetched.
  // |commands_fetched_callback| will be executed after the fetch is processed.
  void ExpectFetchCommands(size_t expected_command_results,
                           size_t expected_fetched_commands,
                           const base::Closure& commands_fetched_callback) {
    expected_fetch_commands_calls_.push(FetchCallExpectation(
        expected_command_results, expected_fetched_commands,
        commands_fetched_callback));
  }

 private:
  // Expectations for a single FetchRemoteCommands() call.
  struct FetchCallExpectation {
    FetchCallExpectation(size_t expected_command_results,
                         size_t expected_fetched_commands,
                         const base::Closure& commands_fetched_callback)
        : expected_command_results(expected_command_results),
          expected_fetched_commands(expected_fetched_commands),
          commands_fetched_callback(commands_fetched_callback) {}
    virtual ~FetchCallExpectation() {}

    const size_t expected_command_results;
    const size_t expected_fetched_commands;
    const base::Closure commands_fetched_callback;
  };

  void FetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<em::RemoteCommandResult>& command_results,
      const RemoteCommandCallback& callback) override {
    ASSERT_FALSE(expected_fetch_commands_calls_.empty());

    const FetchCallExpectation fetch_call_expectation =
        expected_fetch_commands_calls_.front();
    expected_fetch_commands_calls_.pop();

    // Simulate delay from client to DMServer.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &TestingCloudPolicyClientForRemoteCommands::DoFetchRemoteCommands,
            base::Unretained(this), base::Passed(&last_command_id),
            command_results, callback, fetch_call_expectation),
        base::TimeDelta::FromSeconds(
            kTestClientServerCommunicationDelayInSeconds));
  }

  void DoFetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<em::RemoteCommandResult>& command_results,
      const RemoteCommandCallback& callback,
      const FetchCallExpectation& fetch_call_expectation) {
    const std::vector<em::RemoteCommand> fetched_commands =
        server_->FetchCommands(std::move(last_command_id), command_results);

    EXPECT_EQ(fetch_call_expectation.expected_command_results,
              command_results.size());
    EXPECT_EQ(fetch_call_expectation.expected_fetched_commands,
              fetched_commands.size());

    if (!fetch_call_expectation.commands_fetched_callback.is_null())
      fetch_call_expectation.commands_fetched_callback.Run();

    // Simulate delay from DMServer back to client.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(callback, DM_STATUS_SUCCESS, fetched_commands),
        base::TimeDelta::FromSeconds(
            kTestClientServerCommunicationDelayInSeconds));
  }

  std::queue<FetchCallExpectation> expected_fetch_commands_calls_;
  TestingRemoteCommandsServer* server_;

  DISALLOW_COPY_AND_ASSIGN(TestingCloudPolicyClientForRemoteCommands);
};

// A scoped TestMockTimeTaskRunner capable to run tasks until Quit() is called.
class ScopedMockTimeTaskRunner : public base::TestMockTimeTaskRunner {
 public:
  class ScopedRunner {
   public:
    explicit ScopedRunner(
        const scoped_refptr<ScopedMockTimeTaskRunner>& task_runner)
        : task_runner_(task_runner) {
      DCHECK(!task_runner_->attached_runner_);
      task_runner_->attached_runner_ = this;
    }

    virtual ~ScopedRunner() {
      DCHECK_EQ(this, task_runner_->attached_runner_);
      DCHECK(run_called_);
      DCHECK(quit_called_);

      task_runner_->attached_runner_ = nullptr;
    }

    void Run() {
      DCHECK(!run_called_);
      run_called_ = true;

      // It's okay to call Quit() before calling Run().
      if (quit_called_)
        return;
      task_runner_->FastForwardUntilNoTasksRemain();
    }

    void Quit() {
      DCHECK(!quit_called_);
      quit_called_ = true;
    }

    base::Closure QuitClosure() {
      // It's safe to use Unretained here since Quit() is required to be
      // called before dtor is called.
      return base::Bind(&ScopedRunner::Quit, base::Unretained(this));
    }

   private:
    friend class ScopedMockTimeTaskRunner;

    bool run_called_ = false;
    bool quit_called_ = false;

    scoped_refptr<ScopedMockTimeTaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(ScopedRunner);
  };

  ScopedMockTimeTaskRunner() {}

 private:
  ~ScopedMockTimeTaskRunner() override { DCHECK(!attached_runner_); }

  bool IsElapsingStopped() override {
    return attached_runner_ && attached_runner_->quit_called_;
  }

  // Points to the current attached ScopedRunner, and is null if no runner is
  // attached.
  ScopedRunner* attached_runner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedMockTimeTaskRunner);
};

// Base class for unit tests regarding remote commands service.
class RemoteCommandsServiceTest : public testing::Test {
 protected:
  RemoteCommandsServiceTest()
      : task_runner_(new ScopedMockTimeTaskRunner()),
        runner_handle_(task_runner_) {}

  void SetUp() override {
    server_.reset(new TestingRemoteCommandsServer());
    server_->SetClock(task_runner_->GetMockTickClock());
    cloud_policy_client_.reset(
        new TestingCloudPolicyClientForRemoteCommands(server_.get()));
  }

  void TearDown() override {
    remote_commands_service_.reset();
    cloud_policy_client_.reset();
    server_.reset();
  }

  void StartService(std::unique_ptr<RemoteCommandsFactory> factory) {
    remote_commands_service_.reset(new RemoteCommandsService(
        std::move(factory), cloud_policy_client_.get()));
    remote_commands_service_->SetClockForTesting(
        task_runner_->GetMockTickClock());
  }

  void FlushAllTasks() {
    task_runner_->FastForwardUntilNoTasksRemain();
  }

  std::unique_ptr<TestingRemoteCommandsServer> server_;
  std::unique_ptr<TestingCloudPolicyClientForRemoteCommands>
      cloud_policy_client_;
  std::unique_ptr<RemoteCommandsService> remote_commands_service_;

  scoped_refptr<ScopedMockTimeTaskRunner> task_runner_;

 private:
  base::ThreadTaskRunnerHandle runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCommandsServiceTest);
};

// Tests that no command will be fetched if no commands is issued.
TEST_F(RemoteCommandsServiceTest, NoCommands) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(0);

  StartService(std::move(factory));

  // A fetch requst should get nothing from server.
  cloud_policy_client_->ExpectFetchCommands(0u, 0u, base::Closure());
  EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

  FlushAllTasks();
}

// Tests that existing commands issued before service started will be fetched.
TEST_F(RemoteCommandsServiceTest, ExistingCommand) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  {
    ScopedMockTimeTaskRunner::ScopedRunner scoped_runner(task_runner_);

    // Issue a command before service started.
    server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST,
                          kTestPayload,
                          base::Bind(&ExpectSucceededJob, kTestPayload), false);

    // Start the service, run until the command is fetched.
    cloud_policy_client_->ExpectFetchCommands(0u, 1u,
                                              scoped_runner.QuitClosure());
    StartService(std::move(factory));
    EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

    scoped_runner.Run();
  }

  // And run again so that the result can be reported.
  cloud_policy_client_->ExpectFetchCommands(1u, 0u, base::Closure());

  FlushAllTasks();

  EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
}

// Tests that commands issued after service started will be fetched.
TEST_F(RemoteCommandsServiceTest, NewCommand) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  StartService(std::move(factory));

  // Set up expectations on fetch commands calls. The first request will fetch
  // one command, and the second will fetch none but provide result for the
  // previous command instead.
  cloud_policy_client_->ExpectFetchCommands(0u, 1u, base::Closure());
  cloud_policy_client_->ExpectFetchCommands(1u, 0u, base::Closure());

  // Issue a command and manually start a command fetch.
  server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST, kTestPayload,
                        base::Bind(&ExpectSucceededJob, kTestPayload), false);
  EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

  FlushAllTasks();

  EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
}

// Tests that commands issued after service started will be fetched, even if
// the command is issued when a fetch request is ongoing.
TEST_F(RemoteCommandsServiceTest, NewCommandFollwingFetch) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  StartService(std::move(factory));

  {
    ScopedMockTimeTaskRunner::ScopedRunner scoped_runner(task_runner_);

    // Add a command which will be issued after first fetch.
    server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST,
                          kTestPayload,
                          base::Bind(&ExpectSucceededJob, kTestPayload), true);

    cloud_policy_client_->ExpectFetchCommands(0u, 0u,
                                              scoped_runner.QuitClosure());

    // Attempts to fetch commands.
    EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

    // There should be no issued command at this point.
    EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());

    // The command fetch should be in progress.
    EXPECT_TRUE(remote_commands_service_->IsCommandFetchInProgressForTesting());

    // And a following up fetch request should be enqueued.
    EXPECT_FALSE(remote_commands_service_->FetchRemoteCommands());

    // Run until first fetch request is completed.
    scoped_runner.Run();
  }

  // The command should be issued now. Note that this command was actually
  // issued before the first fetch request completes in previous run loop.
  EXPECT_EQ(1u, server_->NumberOfCommandsPendingResult());

  cloud_policy_client_->ExpectFetchCommands(0u, 1u, base::Closure());
  cloud_policy_client_->ExpectFetchCommands(1u, 0u, base::Closure());

  // No further fetch request is made, but the new issued command should be
  // fetched and executed.
  FlushAllTasks();

  EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
}

}  // namespace policy
