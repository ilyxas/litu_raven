#pragma once

namespace raven {

// Product composition root. It constructs long-lived runtime objects, wires
// their dependencies, and starts owner tasks in dependency order.
void start_product_runtime();

} // namespace raven
