#ifndef PACKCONTROLLER_CORE_SCHEDULER_HPP
#define PACKCONTROLLER_CORE_SCHEDULER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace packcontroller::core {

using TaskCallback = void (*)(void* context);
using TimeSource = std::uint32_t (*)(void* context);

struct TaskConfig final {
  const char* name;
  std::uint32_t period_us;
  bool critical;
  TaskCallback callback;
  void* context;
};

struct TaskMetrics final {
  std::uint32_t last_runtime_us{0U};
  std::uint32_t max_runtime_us{0U};
  std::uint32_t max_start_lateness_us{0U};
  std::uint32_t deadline_misses{0U};
  std::uint32_t consecutive_deadline_misses{0U};
  std::uint32_t overrun_limit_violations{0U};
  std::uint32_t skipped_releases{0U};
  std::uint32_t run_count{0U};
  std::uint32_t last_complete_us{0U};
};

class Scheduler final {
 public:
  static constexpr std::size_t kMaxTasks = 8U;

  Scheduler(TimeSource time_source, void* time_context) noexcept;

  bool add_task(const TaskConfig& config) noexcept;
  void start(std::uint32_t now_us) noexcept;
  void run_due_tasks() noexcept;
  void reset_report_window() noexcept;

  [[nodiscard]] std::size_t task_count() const noexcept;
  [[nodiscard]] const TaskConfig& config(std::size_t index) const noexcept;
  [[nodiscard]] const TaskMetrics& metrics(std::size_t index) const noexcept;

 private:
  struct TaskSlot final {
    TaskConfig config{};
    TaskMetrics metrics{};
    std::uint32_t next_release_us{0U};
  };

  TimeSource time_source_;
  void* time_context_;
  std::array<TaskSlot, kMaxTasks> tasks_{};
  std::size_t task_count_{0U};
  bool started_{false};
};

class WatchdogHealthGate final {
 public:
  static constexpr std::uint32_t kCompletionToleranceUs = 1000U;

  [[nodiscard]] bool allow_edge(const Scheduler& scheduler,
                                std::uint32_t now_us) noexcept;
  [[nodiscard]] bool healthy() const noexcept;
  void reset() noexcept;

 private:
  std::array<std::uint32_t, Scheduler::kMaxTasks> acknowledged_skips_{};
  std::array<std::uint32_t, Scheduler::kMaxTasks>
      acknowledged_overrun_violations_{};
  bool latched_healthy_{true};
};

}  // namespace packcontroller::core

#endif
