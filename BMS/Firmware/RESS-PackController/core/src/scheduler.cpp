#include "packcontroller/core/scheduler.hpp"

#include <limits>

#include "packcontroller/core/time.hpp"

namespace packcontroller::core {
namespace {

std::uint32_t saturating_add(std::uint32_t value,
                             std::uint32_t increment) noexcept {
  const auto maximum = std::numeric_limits<std::uint32_t>::max();
  if (increment > (maximum - value)) {
    return maximum;
  }
  return value + increment;
}

}  // namespace

Scheduler::Scheduler(TimeSource time_source, void* time_context) noexcept
    : time_source_(time_source), time_context_(time_context) {}

bool Scheduler::add_task(const TaskConfig& config) noexcept {
  if (started_ || (task_count_ >= tasks_.size()) ||
      (config.period_us == 0U) || (config.callback == nullptr)) {
    return false;
  }
  std::size_t insertion_index = task_count_;
  while ((insertion_index > 0U) &&
         (config.period_us <
          tasks_[insertion_index - 1U].config.period_us)) {
    tasks_[insertion_index] = tasks_[insertion_index - 1U];
    --insertion_index;
  }
  tasks_[insertion_index] = TaskSlot{config, TaskMetrics{}, 0U};
  ++task_count_;
  return true;
}

void Scheduler::start(std::uint32_t now_us) noexcept {
  for (std::size_t index = 0U; index < task_count_; ++index) {
    tasks_[index].next_release_us = now_us + tasks_[index].config.period_us;
  }
  started_ = true;
}

void Scheduler::run_due_tasks() noexcept {
  if (!started_ || (time_source_ == nullptr)) {
    return;
  }

  const std::uint32_t dispatch_us = time_source_(time_context_);
  for (std::size_t index = 0U; index < task_count_; ++index) {
    auto& task = tasks_[index];
    if (!time_due(dispatch_us, task.next_release_us)) {
      continue;
    }

    const std::uint32_t start_us = time_source_(time_context_);
    const std::uint32_t start_lateness_us = start_us - task.next_release_us;
    const std::uint32_t skipped = start_lateness_us / task.config.period_us;
    task.metrics.skipped_releases =
        saturating_add(task.metrics.skipped_releases, skipped);
    task.next_release_us += (skipped + 1U) * task.config.period_us;

    task.config.callback(task.config.context);
    const std::uint32_t complete_us = time_source_(time_context_);
    const std::uint32_t runtime_us = complete_us - start_us;

    task.metrics.last_runtime_us = runtime_us;
    if (runtime_us > task.metrics.max_runtime_us) {
      task.metrics.max_runtime_us = runtime_us;
    }
    if (start_lateness_us > task.metrics.max_start_lateness_us) {
      task.metrics.max_start_lateness_us = start_lateness_us;
    }
    if ((start_lateness_us + runtime_us) > task.config.period_us) {
      task.metrics.deadline_misses =
          saturating_add(task.metrics.deadline_misses, 1U);
      task.metrics.consecutive_deadline_misses = saturating_add(
          task.metrics.consecutive_deadline_misses, 1U);
      if (task.metrics.consecutive_deadline_misses == 2U) {
        task.metrics.overrun_limit_violations = saturating_add(
            task.metrics.overrun_limit_violations, 1U);
      }
    } else {
      task.metrics.consecutive_deadline_misses = 0U;
    }
    task.metrics.run_count = saturating_add(task.metrics.run_count, 1U);
    task.metrics.last_complete_us = complete_us;
  }
}

void Scheduler::reset_report_window() noexcept {
  for (std::size_t index = 0U; index < task_count_; ++index) {
    tasks_[index].metrics.max_runtime_us = 0U;
    tasks_[index].metrics.max_start_lateness_us = 0U;
  }
}

std::size_t Scheduler::task_count() const noexcept { return task_count_; }

const TaskConfig& Scheduler::config(std::size_t index) const noexcept {
  return tasks_[index].config;
}

const TaskMetrics& Scheduler::metrics(std::size_t index) const noexcept {
  return tasks_[index].metrics;
}

bool WatchdogHealthGate::allow_edge(const Scheduler& scheduler,
                                    std::uint32_t now_us) noexcept {
  if (!latched_healthy_) {
    return false;
  }

  for (std::size_t index = 0U; index < scheduler.task_count(); ++index) {
    const auto& config = scheduler.config(index);
    if (!config.critical) {
      continue;
    }

    const auto& metrics = scheduler.metrics(index);
    const bool completed = metrics.run_count != 0U;
    const bool completion_fresh =
        completed &&
        ((now_us - metrics.last_complete_us) <=
         (config.period_us + kCompletionToleranceUs));
    const bool no_skips =
        metrics.skipped_releases == acknowledged_skips_[index];
    const bool overrun_limit_valid =
        metrics.overrun_limit_violations ==
        acknowledged_overrun_violations_[index];

    if (!completion_fresh || !no_skips || !overrun_limit_valid) {
      latched_healthy_ = false;
      return false;
    }
  }

  for (std::size_t index = 0U; index < scheduler.task_count(); ++index) {
    acknowledged_skips_[index] = scheduler.metrics(index).skipped_releases;
    acknowledged_overrun_violations_[index] =
        scheduler.metrics(index).overrun_limit_violations;
  }
  return true;
}

bool WatchdogHealthGate::healthy() const noexcept { return latched_healthy_; }

void WatchdogHealthGate::reset() noexcept {
  acknowledged_skips_.fill(0U);
  acknowledged_overrun_violations_.fill(0U);
  latched_healthy_ = true;
}

}  // namespace packcontroller::core
