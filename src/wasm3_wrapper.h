#ifndef WASM3_WRAPPER_H
#define WASM3_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// wasm3 runtime structure
typedef struct {
    void* runtime;
    void* module;
    int initialized;
} wasm3_runtime_t;

// wasm3 configuration
typedef struct {
    size_t stack_size;
    size_t heap_size;
    int enable_tracing;
} wasm3_config_t;

// Function return codes
#define WASM3_SUCCESS 0
#define WASM3_ERROR_INIT_FAILED -1
#define WASM3_ERROR_LOAD_FAILED -2
#define WASM3_ERROR_COMPILE_FAILED -3
#define WASM3_ERROR_EXECUTION_FAILED -4

// Initialize wasm3 runtime
int wasm3_init(wasm3_runtime_t* runtime, const wasm3_config_t* config);

// Load WASM module from binary data
int wasm3_load_module(wasm3_runtime_t* runtime, const uint8_t* wasm_binary, size_t size);

// Compile loaded module
int wasm3_compile_module(wasm3_runtime_t* runtime);

// Call function by name
int wasm3_call_function(wasm3_runtime_t* runtime, const char* function_name, 
                       const void* args, size_t num_args, int* result);

// Cleanup
void wasm3_cleanup(wasm3_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif // WASM3_WRAPPER_H
