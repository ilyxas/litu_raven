#pragma once
namespace raven::rnd {
using TestLog = void (*)(const char*, bool);
bool run_self_tests(TestLog log);
}
