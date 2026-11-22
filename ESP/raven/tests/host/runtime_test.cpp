#include "runtime_self_test.hpp"
#include <cstdio>
int main() {
    return raven::demo::run_self_tests([](const char* name, bool ok) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    }) ? 0 : 1;
}
