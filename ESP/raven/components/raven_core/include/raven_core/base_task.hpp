#pragma once

#include "raven_core/task_message.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace raven {

// BaseTask — minimal C++ wrapper around a FreeRTOS task and its inbound queue.
//
// Derived classes implement handle_message() to process queued messages.
// Business logic always runs inside the owning task context — never directly
// inside callbacks or ISRs.
//
// Typical usage:
//   class MyWorker : public BaseTask {
//   protected:
//       void handle_message(const TaskMessage& msg) override { ... }
//   };
//   MyWorker worker({ "my_worker", 4096, 5, 8 });
//   worker.start();
//   worker.post_message({ kind, id, nullptr });
class BaseTask {
public:
    struct Config {
        const char*  name;          // FreeRTOS task name
        uint32_t     stack_size;    // task stack in bytes
        UBaseType_t  priority;      // FreeRTOS task priority
        UBaseType_t  queue_length;  // inbound queue capacity (messages)
    };

    explicit BaseTask(const Config& cfg);
    virtual ~BaseTask();

    BaseTask(const BaseTask&)            = delete;
    BaseTask& operator=(const BaseTask&) = delete;

    // Create the inbound queue and spawn the FreeRTOS task.
    // Must be called only once.
    void start();

    // Enqueue a message for processing in the owning task context.
    // Must only be called after start().
    // Returns true if the message was accepted before the timeout expired.
    bool post_message(const TaskMessage& msg, TickType_t timeout = portMAX_DELAY);

protected:
    // Called once inside the task, before the receive loop begins.
    // Override in derived classes to perform task-local initialisation.
    virtual void on_start() {}

    // Called for each message dequeued from the inbound queue.
    // All non-trivial processing must happen here, in the owning task context.
    virtual void handle_message(const TaskMessage& msg) = 0;

    // Enable periodic ticking at the specified interval.
    // Call from on_start() or handle_message() (i.e. within the task context).
    // interval_ticks == 0 disables ticking (default — queue blocks indefinitely).
    void set_tick_interval(TickType_t interval_ticks);

    // Called at approximately the configured tick interval when the queue is idle.
    // Default implementation does nothing.
    // Override in derived classes to implement periodic polling behaviour.
    virtual void on_tick() {}

private:
    // Static trampoline: recovers the BaseTask instance and calls run().
    static void task_entry(void* arg);

    // Internal receive loop. Blocks on the queue, with optional tick timeout.
    void run();

    Config        cfg_;
    TaskHandle_t  task_handle_;
    QueueHandle_t queue_;

    // Periodic tick state — only accessed from within the task's run() loop.
    TickType_t    tick_interval_ticks_{0};
    TickType_t    last_tick_time_{0};
};

} // namespace raven
