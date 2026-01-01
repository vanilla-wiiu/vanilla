#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* iOS client never calls backend internals */
/* These are placeholders to satisfy the compiler */

struct vanilla_context { int dummy; };

static inline int vanilla_init(struct vanilla_context *ctx) { return -1; }
static inline void vanilla_shutdown(struct vanilla_context *ctx) {}

/* Add more stubs ONLY if compiler complains */

#ifdef __cplusplus
}
#endif
