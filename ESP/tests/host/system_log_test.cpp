#include "esp_log.h"
#include "raven_platform/connection_failure.hpp"
#include "raven_platform/system_log.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            std::abort();                                                                                    \
        }                                                                                                    \
    } while (0)

namespace {
std::atomic<bool> block_timer{false}, timer_entered{false}, release_timer{false}, keep_logging{true};
std::atomic<unsigned> forwarded{0};
vprintf_like_t installed{};
std::thread concurrent;
int emit(vprintf_like_t callback, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = callback(format, args);
    va_end(args);
    return result;
}
int old_output(const char *format, va_list arguments) {
    char text[256]{};
    const int length = std::vsnprintf(text, sizeof(text), format, arguments);
    ++forwarded;
    return length;
}
} // namespace

int64_t esp_timer_get_time() {
    if (block_timer.exchange(false)) {
        timer_entered = true;
        while (!release_timer.load())
            std::this_thread::yield();
    }
    return 100000;
}
vprintf_like_t esp_log_set_vprintf(vprintf_like_t callback) {
    installed = callback;
    // The hook is already visible, but its installer has not received old_output.
    block_timer = true;
    std::thread first([&] { CHECK(emit(callback, "W (1) startup: %s\n", "first") > 0); });
    while (!timer_entered.load())
        std::this_thread::yield();
    // Exercise the busy capture path before the old callback is available too.
    CHECK(emit(callback, "W (2) startup: %s\n", "concurrent") > 0);
    release_timer = true;
    first.join();
    concurrent = std::thread([callback] {
        while (keep_logging.load()) {
            emit(callback, "I (3) startup: concurrent output\n");
            std::this_thread::yield();
        }
    });
    return old_output;
}
int main() {
    raven::diagnostics::Record records[8]{};
    raven::diagnostics::Journal journal(records, 8);
    raven::capture_system_log(journal, 1, 2);
    keep_logging = false;
    concurrent.join();
    CHECK(emit(installed, "E (4) startup: %s\n", "after install") == 29);
    CHECK(forwarded.load() >= 1 && raven::system_log_drops() >= 1);
    raven::diagnostics::Record record{};
    CHECK(journal.read(journal.latest(), record));
    CHECK(record.device == 1 && record.boot == 2 && record.severity == 3);
    CHECK(!std::strcmp(record.text, "startup: after install"));

    // Exercise the exact latch used by BleSerialServer::disconnect. Old results
    // cannot fail a new connection or erase its already recorded failure.
    std::atomic<uint32_t> generation{4}, failed{0};
    raven::fail_connection_generation(generation, failed, 4);
    CHECK(failed == 4);
    generation = 6;
    failed = 0;
    raven::fail_connection_generation(generation, failed, 4);
    CHECK(failed == 0);
    std::thread old([&] {
        for (unsigned i = 0; i < 10000; ++i)
            raven::fail_connection_generation(generation, failed, 4);
    });
    raven::fail_connection_generation(generation, failed, 6);
    old.join();
    CHECK(failed == 6);
    raven::fail_connection_generation(generation, failed, 4);
    CHECK(failed == 6);
    std::puts("PASS 2 platform log/connection failure scenarios");
}
