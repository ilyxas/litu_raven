#pragma once
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
// Run the real TCP channel/endpoint against POSIX descriptors, substituting only
// recv to reproduce errno and frame-fragment scheduling deterministically.
ssize_t raven_test_recv(int descriptor, void *bytes, size_t size, int flags);
#define recv raven_test_recv
