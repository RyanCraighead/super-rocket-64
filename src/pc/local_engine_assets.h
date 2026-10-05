#ifndef SUPER_ROCKET_LOCAL_ENGINE_ASSETS_H
#define SUPER_ROCKET_LOCAL_ENGINE_ASSETS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers storage, never content. Data is supplied privately at runtime. */
void local_engine_asset_register(void *destination, size_t size, const char *relative_path, const char *sha256);
/* Validates every file before changing any registered buffer. */
int local_engine_assets_load(const char *directory, char *error, size_t error_size);
size_t local_engine_assets_count(void);

#ifdef __cplusplus
}
#endif

#define LOCAL_ENGINE_ASSET_REGISTER(name, path, hash) \
    __attribute__((constructor)) static void name##_local_asset_register(void) { \
        local_engine_asset_register(name, sizeof(name), path, hash); \
    }

#endif
