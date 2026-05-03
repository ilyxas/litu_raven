#pragma once

#include "raven_core/base_task.hpp"

namespace raven {

// BaseActivity — lightweight specialization of BaseTask for orchestration / FSM behavior.
//
// Activities represent the mode the vehicle is currently operating in.
// They own the decision flow for transitioning between modes and orchestrate
// which services are active.
//
// Concrete activities derive from BaseActivity and implement handle_message() to react to
// directed commands and events posted to their inbound queue.
class BaseActivity : public BaseTask {
public:
    using BaseTask::BaseTask;
    using BaseTask::Config;
};

} // namespace raven
