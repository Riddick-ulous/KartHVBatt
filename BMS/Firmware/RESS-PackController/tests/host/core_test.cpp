#include <gtest/gtest.h>

#include <cstdint>

#include "packcontroller/core/faults.hpp"
#include "packcontroller/core/scheduler.hpp"
#include "packcontroller/core/signals.hpp"

namespace packcontroller::core {
namespace {

struct FakeClock final {
  std::uint32_t now_us{0U};
  std::uint32_t callback_runtime_us{0U};
};

std::uint32_t read_fake_time(void* context) {
  return static_cast<FakeClock*>(context)->now_us;
}

std::uint32_t read_and_advance_fake_time(void* context) {
  auto* clock = static_cast<FakeClock*>(context);
  const std::uint32_t result = clock->now_us;
  clock->now_us += clock->callback_runtime_us;
  return result;
}

void advance_fake_time(void* context) {
  auto* clock = static_cast<FakeClock*>(context);
  clock->now_us += clock->callback_runtime_us;
}

struct ExecutionOrder final {
  std::uint32_t entries[2]{0U, 0U};
  std::size_t count{0U};
};

struct OrderedTaskContext final {
  ExecutionOrder* order;
  std::uint32_t id;
};

void record_execution(void* context) {
  auto* task = static_cast<OrderedTaskContext*>(context);
  task->order->entries[task->order->count] = task->id;
  ++task->order->count;
}

TEST(Scheduler, RunsDueTasksFromShortestToLongestPeriod) {
  FakeClock clock{};
  Scheduler scheduler{read_fake_time, &clock};
  ExecutionOrder order{};
  OrderedTaskContext slow{&order, 10U};
  OrderedTaskContext fast{&order, 1U};

  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"10ms", 10000U, true, record_execution, &slow}));
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"1ms", 1000U, true, record_execution, &fast}));
  scheduler.start(0U);
  clock.now_us = 10000U;
  scheduler.run_due_tasks();

  ASSERT_EQ(order.count, 2U);
  EXPECT_EQ(order.entries[0], 1U);
  EXPECT_EQ(order.entries[1], 10U);
  EXPECT_STREQ(scheduler.config(0U).name, "1ms");
  EXPECT_STREQ(scheduler.config(1U).name, "10ms");
}

TEST(Scheduler, UsesOneDueTimeSnapshotAcrossAReleaseBoundary) {
  FakeClock clock{};
  Scheduler scheduler{read_and_advance_fake_time, &clock};
  ExecutionOrder order{};
  OrderedTaskContext slow{&order, 2U};
  OrderedTaskContext fast{&order, 1U};
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"2ms", 2000U, true, record_execution, &slow}));
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"1ms", 1000U, true, record_execution, &fast}));
  scheduler.start(0U);

  clock.now_us = 1000U;
  clock.callback_runtime_us = 0U;
  scheduler.run_due_tasks();
  order.count = 0U;

  clock.now_us = 1999U;
  clock.callback_runtime_us = 2U;
  scheduler.run_due_tasks();
  EXPECT_EQ(order.count, 0U);

  clock.now_us = 2001U;
  clock.callback_runtime_us = 0U;
  scheduler.run_due_tasks();
  ASSERT_EQ(order.count, 2U);
  EXPECT_EQ(order.entries[0], 1U);
  EXPECT_EQ(order.entries[1], 2U);
}

TEST(Scheduler, UsesAbsoluteReleasesAndDoesNotCatchUp) {
  FakeClock clock{};
  Scheduler scheduler{read_fake_time, &clock};
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"critical", 1000U, true, advance_fake_time, &clock}));
  scheduler.start(0U);

  clock.now_us = 1000U;
  clock.callback_runtime_us = 100U;
  scheduler.run_due_tasks();
  EXPECT_EQ(scheduler.metrics(0U).run_count, 1U);
  EXPECT_EQ(scheduler.metrics(0U).last_runtime_us, 100U);

  clock.now_us = 3500U;
  scheduler.run_due_tasks();
  EXPECT_EQ(scheduler.metrics(0U).run_count, 2U);
  EXPECT_EQ(scheduler.metrics(0U).skipped_releases, 1U);
  EXPECT_EQ(scheduler.metrics(0U).deadline_misses, 1U);
  EXPECT_EQ(scheduler.metrics(0U).max_start_lateness_us, 1500U);

  scheduler.run_due_tasks();
  EXPECT_EQ(scheduler.metrics(0U).run_count, 2U);
}

TEST(Scheduler, ResetsOnlyOneSecondWindowMetrics) {
  FakeClock clock{};
  Scheduler scheduler{read_fake_time, &clock};
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"task", 1000U, false, advance_fake_time, &clock}));
  scheduler.start(0U);
  clock.now_us = 1100U;
  clock.callback_runtime_us = 25U;
  scheduler.run_due_tasks();

  scheduler.reset_report_window();
  EXPECT_EQ(scheduler.metrics(0U).max_runtime_us, 0U);
  EXPECT_EQ(scheduler.metrics(0U).max_start_lateness_us, 0U);
  EXPECT_EQ(scheduler.metrics(0U).last_runtime_us, 25U);
  EXPECT_EQ(scheduler.metrics(0U).run_count, 1U);
}

TEST(WatchdogHealthGate, AllowsOnlyFreshCompleteCriticalTasks) {
  FakeClock clock{};
  Scheduler scheduler{read_fake_time, &clock};
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"critical", 1000U, true, advance_fake_time, &clock}));
  scheduler.start(0U);
  WatchdogHealthGate gate{};

  EXPECT_FALSE(gate.allow_edge(scheduler, 0U));
  EXPECT_FALSE(gate.healthy());
  gate.reset();

  clock.now_us = 1000U;
  clock.callback_runtime_us = 100U;
  scheduler.run_due_tasks();
  EXPECT_TRUE(gate.allow_edge(scheduler, clock.now_us));

  clock.now_us = 3500U;
  scheduler.run_due_tasks();
  EXPECT_FALSE(gate.allow_edge(scheduler, clock.now_us));
  EXPECT_FALSE(gate.healthy());

  clock.now_us = 4000U;
  scheduler.run_due_tasks();
  EXPECT_FALSE(gate.allow_edge(scheduler, clock.now_us));
}

TEST(WatchdogHealthGate, AllowsOneOverrunAndCleanRunResetsStrike) {
  FakeClock clock{};
  Scheduler scheduler{read_fake_time, &clock};
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"critical", 1000U, true, advance_fake_time, &clock}));
  scheduler.start(0U);
  WatchdogHealthGate gate{};

  clock.now_us = 1000U;
  clock.callback_runtime_us = 1001U;
  scheduler.run_due_tasks();

  EXPECT_EQ(scheduler.metrics(0U).deadline_misses, 1U);
  EXPECT_EQ(scheduler.metrics(0U).consecutive_deadline_misses, 1U);
  EXPECT_TRUE(gate.allow_edge(scheduler, clock.now_us));

  clock.callback_runtime_us = 100U;
  scheduler.run_due_tasks();
  EXPECT_EQ(scheduler.metrics(0U).consecutive_deadline_misses, 0U);
  EXPECT_TRUE(gate.allow_edge(scheduler, clock.now_us));

  clock.now_us = 3000U;
  clock.callback_runtime_us = 1001U;
  scheduler.run_due_tasks();
  EXPECT_EQ(scheduler.metrics(0U).consecutive_deadline_misses, 1U);
  EXPECT_TRUE(gate.allow_edge(scheduler, clock.now_us));
}

TEST(WatchdogHealthGate, LatchesOnSecondConsecutiveCriticalOverrun) {
  FakeClock clock{};
  Scheduler scheduler{read_fake_time, &clock};
  ASSERT_TRUE(scheduler.add_task(
      TaskConfig{"critical", 1000U, true, advance_fake_time, &clock}));
  scheduler.start(0U);
  WatchdogHealthGate gate{};

  clock.now_us = 1000U;
  clock.callback_runtime_us = 1001U;
  scheduler.run_due_tasks();
  EXPECT_TRUE(gate.allow_edge(scheduler, clock.now_us));

  scheduler.run_due_tasks();
  EXPECT_EQ(scheduler.metrics(0U).consecutive_deadline_misses, 2U);
  EXPECT_EQ(scheduler.metrics(0U).overrun_limit_violations, 1U);
  EXPECT_FALSE(gate.allow_edge(scheduler, clock.now_us));
  EXPECT_FALSE(gate.healthy());
}

enum class TestSignalId : std::uint8_t { kVoltage = 0U, kCount = 1U };

TEST(SignalStore, PreservesValueAndDerivesStaleWithoutMutatingSource) {
  SignalStore<TestSignalId, std::int32_t,
              static_cast<std::size_t>(TestSignalId::kCount)>
      store{};
  store.publish(TestSignalId::kVoltage, 4200, 100U);

  const auto fresh = store.read(TestSignalId::kVoltage, 600U, 500U);
  EXPECT_EQ(fresh.value, 4200);
  EXPECT_EQ(fresh.quality, SignalQuality::kValid);

  const auto stale = store.read(TestSignalId::kVoltage, 601U, 500U);
  EXPECT_EQ(stale.value, 4200);
  EXPECT_EQ(stale.quality, SignalQuality::kStale);

  const auto original = store.read(TestSignalId::kVoltage, 200U, 500U);
  EXPECT_EQ(original.quality, SignalQuality::kValid);
}

TEST(SignalStore, HandlesTimestampWrapAndExplicitFaultQuality) {
  SignalStore<TestSignalId, std::uint16_t,
              static_cast<std::size_t>(TestSignalId::kCount)>
      store{};
  store.publish(TestSignalId::kVoltage, 7U, UINT32_MAX - 5U);
  EXPECT_EQ(store.read(TestSignalId::kVoltage, 3U, 9U).quality,
            SignalQuality::kValid);
  EXPECT_EQ(store.read(TestSignalId::kVoltage, 4U, 9U).quality,
            SignalQuality::kStale);

  store.mark_fault(TestSignalId::kVoltage, 10U);
  EXPECT_EQ(store.read(TestSignalId::kVoltage, 1000U, 1U).quality,
            SignalQuality::kFault);
}

TEST(FaultManager, AppliesAutoCanAndPowerCycleLatchPolicies) {
  FaultManager manager{};
  const auto warning = static_cast<FaultId>(11U);
  const auto can_fault = static_cast<FaultId>(20U);

  manager.update(warning, FaultSeverity::kWarning,
                 FaultResetPolicy::kAutoClear, true, 10U);
  manager.update(warning, FaultSeverity::kWarning,
                 FaultResetPolicy::kAutoClear, false, 20U);
  EXPECT_FALSE(manager.get(warning).active);
  EXPECT_FALSE(manager.get(warning).latched);
  EXPECT_EQ(manager.get(warning).occurrence_count, 1U);

  manager.update(can_fault, FaultSeverity::kControlledCritical,
                 FaultResetPolicy::kCanResettable, true, 30U);
  EXPECT_TRUE(manager.critical_error_active());
  EXPECT_FALSE(manager.clear_can_latch(can_fault));
  manager.update(can_fault, FaultSeverity::kControlledCritical,
                 FaultResetPolicy::kCanResettable, false, 40U);
  EXPECT_TRUE(manager.get(can_fault).latched);
  EXPECT_TRUE(manager.clear_can_latch(can_fault));
  EXPECT_FALSE(manager.critical_error_active());

  manager.update(FaultId::kSchedulerHealthLoss,
                 FaultSeverity::kStmHardfault,
                 FaultResetPolicy::kPowerCycle, true, 50U);
  manager.update(FaultId::kSchedulerHealthLoss,
                 FaultSeverity::kStmHardfault,
                 FaultResetPolicy::kPowerCycle, false, 60U);
  EXPECT_TRUE(manager.get(FaultId::kSchedulerHealthLoss).latched);
  EXPECT_FALSE(manager.clear_can_latch(FaultId::kSchedulerHealthLoss));
  EXPECT_TRUE(manager.critical_error_active());
}

TEST(FaultManager, BuildsBothHalvesOfActiveAndLatchedBitmaps) {
  FaultManager manager{};
  const auto high_fault = static_cast<FaultId>(70U);
  manager.update(FaultId::kSchedulerTaskOverrun, FaultSeverity::kWarning,
                 FaultResetPolicy::kAutoClear, true, 1U);
  manager.update(high_fault, FaultSeverity::kHvHardfault,
                 FaultResetPolicy::kCanResettable, true, 2U);

  EXPECT_EQ(manager.active_bitmap().low, UINT64_C(1) << 4U);
  EXPECT_EQ(manager.active_bitmap().high, UINT64_C(1) << (70U - 64U));
  EXPECT_TRUE(manager.hv_hardfault_active());
}

}  // namespace
}  // namespace packcontroller::core
