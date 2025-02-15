// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_thread_pool_impl.h"

#include <stddef.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/sequence_sort_key.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/task_scheduler/test_task_factory.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

const size_t kNumThreadsInThreadPool = 4;
const size_t kNumThreadsPostingTasks = 4;
const size_t kNumTasksPostedPerThread = 150;

class TestDelayedTaskManager : public DelayedTaskManager {
 public:
  TestDelayedTaskManager() : DelayedTaskManager(Bind(&DoNothing)) {}

  void SetCurrentTime(TimeTicks now) { now_ = now; }

  // DelayedTaskManager:
  TimeTicks Now() const override { return now_; }

 private:
  TimeTicks now_ = TimeTicks::Now();

  DISALLOW_COPY_AND_ASSIGN(TestDelayedTaskManager);
};

class TaskSchedulerThreadPoolImplTest
    : public testing::TestWithParam<ExecutionMode> {
 protected:
  TaskSchedulerThreadPoolImplTest() = default;

  void SetUp() override {
    thread_pool_ = SchedulerThreadPoolImpl::Create(
        ThreadPriority::NORMAL, kNumThreadsInThreadPool,
        Bind(&TaskSchedulerThreadPoolImplTest::ReEnqueueSequenceCallback,
             Unretained(this)),
        &task_tracker_, &delayed_task_manager_);
    ASSERT_TRUE(thread_pool_);
  }

  void TearDown() override {
    thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
    thread_pool_->JoinForTesting();
  }

  std::unique_ptr<SchedulerThreadPoolImpl> thread_pool_;

  TaskTracker task_tracker_;
  TestDelayedTaskManager delayed_task_manager_;

 private:
  void ReEnqueueSequenceCallback(scoped_refptr<Sequence> sequence) {
    // In production code, this callback would be implemented by the
    // TaskScheduler which would first determine which PriorityQueue the
    // sequence must be re-enqueued.
    const SequenceSortKey sort_key(sequence->GetSortKey());
    thread_pool_->ReEnqueueSequence(std::move(sequence), sort_key);
  }

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerThreadPoolImplTest);
};

using PostNestedTask = test::TestTaskFactory::PostNestedTask;

class ThreadPostingTasks : public SimpleThread {
 public:
  enum class WaitBeforePostTask {
    NO_WAIT,
    WAIT_FOR_ALL_THREADS_IDLE,
  };

  // Constructs a thread that posts tasks to |thread_pool| through an
  // |execution_mode| task runner. If |wait_before_post_task| is
  // WAIT_FOR_ALL_THREADS_IDLE, the thread waits until all worker threads in
  // |thread_pool| are idle before posting a new task. If |post_nested_task| is
  // YES, each task posted by this thread posts another task when it runs.
  ThreadPostingTasks(SchedulerThreadPoolImpl* thread_pool,
                     ExecutionMode execution_mode,
                     WaitBeforePostTask wait_before_post_task,
                     PostNestedTask post_nested_task)
      : SimpleThread("ThreadPostingTasks"),
        thread_pool_(thread_pool),
        wait_before_post_task_(wait_before_post_task),
        post_nested_task_(post_nested_task),
        factory_(thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(),
                                                          execution_mode),
                 execution_mode) {
    DCHECK(thread_pool_);
  }

  const test::TestTaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksOnCurrentThread());

    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i) {
      if (wait_before_post_task_ ==
          WaitBeforePostTask::WAIT_FOR_ALL_THREADS_IDLE) {
        thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
      }
      EXPECT_TRUE(factory_.PostTask(post_nested_task_, nullptr));
    }
  }

  SchedulerThreadPoolImpl* const thread_pool_;
  const scoped_refptr<TaskRunner> task_runner_;
  const WaitBeforePostTask wait_before_post_task_;
  const PostNestedTask post_nested_task_;
  test::TestTaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasks);
};

using WaitBeforePostTask = ThreadPostingTasks::WaitBeforePostTask;

void ShouldNotRunCallback() {
  ADD_FAILURE() << "Ran a task that shouldn't run.";
}

void SignalEventCallback(WaitableEvent* event) {
  DCHECK(event);
  event->Signal();
}

}  // namespace

TEST_P(TaskSchedulerThreadPoolImplTest, PostTasks) {
  // Create threads to post tasks.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(WrapUnique(new ThreadPostingTasks(
        thread_pool_.get(), GetParam(), WaitBeforePostTask::NO_WAIT,
        PostNestedTask::NO)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolImplTest, PostTasksWaitAllThreadsIdle) {
  // Create threads to post tasks. To verify that worker threads can sleep and
  // be woken up when new tasks are posted, wait for all threads to become idle
  // before posting a new task.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(WrapUnique(new ThreadPostingTasks(
        thread_pool_.get(), GetParam(),
        WaitBeforePostTask::WAIT_FOR_ALL_THREADS_IDLE, PostNestedTask::NO)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolImplTest, NestedPostTasks) {
  // Create threads to post tasks. Each task posted by these threads will post
  // another task when it runs.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(WrapUnique(new ThreadPostingTasks(
        thread_pool_.get(), GetParam(), WaitBeforePostTask::NO_WAIT,
        PostNestedTask::YES)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolImplTest, PostTasksWithOneAvailableThread) {
  // Post tasks to keep all threads busy except one until |event| is signaled.
  // Use different factories so that tasks are added to different sequences and
  // can run simultaneously when the execution mode is SEQUENCED.
  WaitableEvent event(true, false);
  std::vector<std::unique_ptr<test::TestTaskFactory>> blocked_task_factories;
  for (size_t i = 0; i < (kNumThreadsInThreadPool - 1); ++i) {
    blocked_task_factories.push_back(WrapUnique(new test::TestTaskFactory(
        thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam()),
        GetParam())));
    EXPECT_TRUE(
        blocked_task_factories.back()->PostTask(PostNestedTask::NO, &event));
    blocked_task_factories.back()->WaitForAllTasksToRun();
  }

  // Post |kNumTasksPostedPerThread| tasks that should all run despite the fact
  // that only one thread in |thread_pool_| isn't busy.
  test::TestTaskFactory short_task_factory(
      thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam()),
      GetParam());
  for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
    EXPECT_TRUE(short_task_factory.PostTask(PostNestedTask::NO, nullptr));
  short_task_factory.WaitForAllTasksToRun();

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolImplTest, Saturate) {
  // Verify that it is possible to have |kNumThreadsInThreadPool|
  // tasks/sequences running simultaneously. Use different factories so that
  // tasks are added to different sequences and can run simultaneously when the
  // execution mode is SEQUENCED.
  WaitableEvent event(true, false);
  std::vector<std::unique_ptr<test::TestTaskFactory>> factories;
  for (size_t i = 0; i < kNumThreadsInThreadPool; ++i) {
    factories.push_back(WrapUnique(new test::TestTaskFactory(
        thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam()),
        GetParam())));
    EXPECT_TRUE(factories.back()->PostTask(PostNestedTask::NO, &event));
    factories.back()->WaitForAllTasksToRun();
  }

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

// Verify that a Task can't be posted after shutdown.
TEST_P(TaskSchedulerThreadPoolImplTest, PostTaskAfterShutdown) {
  auto task_runner =
      thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam());
  task_tracker_.Shutdown();
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, Bind(&ShouldNotRunCallback)));
}

// Verify that a Task posted with a delay is added to the DelayedTaskManager and
// doesn't run before its delay expires.
TEST_P(TaskSchedulerThreadPoolImplTest, PostDelayedTask) {
  EXPECT_TRUE(delayed_task_manager_.GetDelayedRunTime().is_null());

  // Post a delayed task.
  WaitableEvent task_ran(true, false);
  EXPECT_TRUE(thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam())
                  ->PostDelayedTask(FROM_HERE, Bind(&SignalEventCallback,
                                                    Unretained(&task_ran)),
                                    TimeDelta::FromSeconds(10)));

  // The task should have been added to the DelayedTaskManager.
  EXPECT_FALSE(delayed_task_manager_.GetDelayedRunTime().is_null());

  // The task shouldn't run.
  EXPECT_FALSE(task_ran.IsSignaled());

  // Fast-forward time and post tasks that are ripe for execution.
  delayed_task_manager_.SetCurrentTime(
      delayed_task_manager_.GetDelayedRunTime());
  delayed_task_manager_.PostReadyTasks();

  // The task should run.
  task_ran.Wait();
}

INSTANTIATE_TEST_CASE_P(Parallel,
                        TaskSchedulerThreadPoolImplTest,
                        ::testing::Values(ExecutionMode::PARALLEL));
INSTANTIATE_TEST_CASE_P(Sequenced,
                        TaskSchedulerThreadPoolImplTest,
                        ::testing::Values(ExecutionMode::SEQUENCED));

}  // namespace internal
}  // namespace base
