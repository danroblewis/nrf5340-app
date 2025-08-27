#ifndef WASM3_WRAPPER_H
#define WASM3_WRAPPER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <stdbool.h>

// Simplified wasm3 runtime handle (placeholder for now)
typedef struct {
    void *env;
    void *runtime;
    void *module;
    bool is_initialized;
    bool is_loaded;
    bool is_compiled;
} wasm3_runtime_t;

// wasm3 configuration
typedef struct {
    uint32_t stack_size;
    uint32_t heap_size;
    bool enable_tracing;
} wasm3_config_t;

// Function prototypes
int wasm3_init(wasm3_runtime_t *runtime, const wasm3_config_t *config);
int wasm3_load_module(wasm3_runtime_t *runtime, const uint8_t *wasm_binary, size_t size);
int wasm3_compile_module(wasm3_runtime_t *runtime);
int wasm3_call_function(wasm3_runtime_t *runtime, const char *function_name, 
                       const void *args, size_t num_args, void *result);
void wasm3_cleanup(wasm3_runtime_t *runtime);

// Utility functions
void wasm3_print_error(const char *prefix);
bool wasm3_is_initialized(const wasm3_runtime_t *runtime);

#endif // WASM3_WRAPPER_H
