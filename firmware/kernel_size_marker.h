#pragma once
#include <circle/sysconfig.h>
#define GROOVE_STRINGIFY_INNER(value) #value
#define GROOVE_STRINGIFY(value) GROOVE_STRINGIFY_INNER(value)
// Record the actual Circle configuration in the linked ELF for a host check.
__asm__(".global groove_kernel_max_size\n.set groove_kernel_max_size, " GROOVE_STRINGIFY(KERNEL_MAX_SIZE));
__asm__(".global groove_kernel_stack_size\n.set groove_kernel_stack_size, " GROOVE_STRINGIFY(KERNEL_STACK_SIZE));
#undef GROOVE_STRINGIFY
#undef GROOVE_STRINGIFY_INNER
