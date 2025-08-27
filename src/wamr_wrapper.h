#ifndef WAMR_WRAPPER_H
#define WAMR_WRAPPER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <stdbool.h>

// WAMR runtime handle
typedef struct {
    void *runtime;
    void *module;
    void *module_inst;
    void *exec_env;
    bool is_initialized;
    bool is_loaded;
    bool is_instantiated;
} wamr_runtime_t;

// WAMR configuration
typedef struct {
    uint32_t stack_size;
    uint32_t heap_size;
    uint32_t max_memory_pages;
    bool enable_gc;
    bool enable_simd;
    bool enable_ref_types;
} wamr_config_t;

// Function prototypes
int wamr_init(wamr_runtime_t *runtime, const wamr_config_t *config);
int wamr_load_module(wamr_runtime_t *runtime, const uint8_t *wasm_binary, size_t size);
int wamr_instantiate_module(wamr_runtime_t *runtime);
int wamr_call_function(wamr_runtime_t *runtime, const char *function_name, 
                      const void *args, size_t num_args, void *result);
void wamr_cleanup(wamr_runtime_t *runtime);

// Utility functions
void wamr_print_error(const char *prefix);
bool wamr_is_initialized(const wamr_runtime_t *runtime);

#endif // WAMR_WRAPPER_H
