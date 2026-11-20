#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdio.h>
#include "../../build_config.h"

#if VANILLA_DIAGNOSTICS
// For even more verbose printing with VANILLA_DIAGNOSTICS, set this to true
static constexpr bool network_debug_enabled = false;

#define NETWORK_DEBUG(...)            \
  do {                                \
    if (network_debug_enabled) {      \
      printf(__VA_ARGS__);            \
    }                                 \
  } while (0)
#else
#define NETWORK_DEBUG(...) do {} while (0)
#endif

#endif // DEBUG_LOG_H
