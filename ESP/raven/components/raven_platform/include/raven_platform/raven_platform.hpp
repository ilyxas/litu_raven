#pragma once


// raven_platform: hardware and ESP-IDF platform abstractions.
// Provides thin wrappers and initialisation helpers that isolate the
// rest of the runtime from direct ESP-IDF driver dependencies.

#include "raven_platform/ILink.hpp"
#include "raven_platform/TcpClientLink.hpp"
#include "raven_platform/TcpServer.hpp"

