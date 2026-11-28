#pragma once
#include <cstdlib>
constexpr int ESP_OK = 0, ESP_ERR_NO_MEM = -1;
#define ESP_ERROR_CHECK(result)                                                                              \
    do {                                                                                                     \
        if ((result) != ESP_OK)                                                                              \
            std::abort();                                                                                    \
    } while (0)
