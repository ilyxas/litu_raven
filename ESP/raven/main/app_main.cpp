#include "flight_controller_bootstrap.hpp"
#include "demo_runtime.hpp"
#include "sdkconfig.h"

extern "C" void app_main()
{
#ifdef CONFIG_RAVEN_RND_RUNTIME
    raven::demo::start_demo_runtime();
#else
    raven::start_product_runtime();
#endif
}
