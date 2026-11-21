#pragma once

#include <cstddef>
#include <cstdint>

// TaskMessage — minimal directed message posted to a task's inbound queue.
//
// kind and id together identify what the message represents.
// data is copied into receiver-owned storage by BaseTask::post_message() when
// payload_size is non-zero. The sender's buffer is borrowed only during that
// enqueue call.
//
// Events (broadcast facts) and task messages (directed commands) are
// intentionally distinct concepts in this runtime.

namespace raven {

struct TaskMessage {
    uint16_t kind;         // message category
    uint16_t id;           // numeric identifier within the category
    void*    data;         // owned payload buffer, may be nullptr
    size_t   payload_size; // size of owned payload buffer in bytes
};

} // namespace raven
