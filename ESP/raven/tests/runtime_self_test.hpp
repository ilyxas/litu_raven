#pragma once
namespace raven::demo {
using TestLog = void (*)(const char*, bool);
bool run_self_tests(TestLog log);
}
