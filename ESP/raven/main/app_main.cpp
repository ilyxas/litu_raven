#include "flight_controller_bootstrap.hpp"
#include "raven_rnd_esp/runtime.hpp"
#include "sdkconfig.h"

extern "C" void app_main()
{
#ifdef CONFIG_RAVEN_RND_RUNTIME
    raven::start_rnd_runtime();
#else
    raven::start_product_runtime();
#endif
}
