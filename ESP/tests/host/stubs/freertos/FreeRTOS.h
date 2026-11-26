#pragma once
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using TaskHandle_t = void *;
constexpr int pdPASS = 1;
#define pdMS_TO_TICKS(ms) (ms)
