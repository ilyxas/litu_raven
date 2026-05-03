#pragma once

#include "raven_core/base_task.hpp"

namespace raven {

// BaseService — lightweight specialization of BaseTask for asynchronous subsystem workers.
//
// Services encapsulate interaction with a single hardware subsystem or software stack.
// They run in their own FreeRTOS task and write results into shared state via controllers.
//
// Concrete services derive from BaseService and implement handle_message() to react to
// directed commands posted to their inbound queue.
class BaseService : public BaseTask {
public:
    using BaseTask::BaseTask;
    using BaseTask::Config;
};

} // namespace raven
