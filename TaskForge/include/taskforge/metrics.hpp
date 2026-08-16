#pragma once

#include <chrono>
#include <cstddef>

namespace taskforge {

struct ExecutorMetrics {
    std::size_t workers{0};
    std::size_t queue_capacity{0};
    std::size_t current_queue_depth{0};
    std::size_t peak_queue_depth{0};
    std::size_t tasks_submitted{0};
    std::size_t tasks_completed{0};
    std::size_t tasks_failed{0};
    std::chrono::nanoseconds elapsed{0};
};

}  // namespace taskforge
