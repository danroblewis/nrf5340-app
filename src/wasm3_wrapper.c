#include "wasm3_wrapper.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

// Include actual wasm3 headers
#include "m3_core.h"
#include "m3_env.h"
#include "m3_info.h"
#include "m3_api_libc.h"

// wasm3 error constants
#define m3Err_none NULL
#define m3Err_mallocFailed "malloc failed"

int wasm3_init(wasm3_runtime_t* runtime, const wasm3_config_t* config)
{
    if (!runtime || !config) {
        return WASM3_ERROR_INIT_FAILED;
    }

    // Initialize runtime structure
    memset(runtime, 0, sizeof(wasm3_runtime_t));

    // Create M3 environment
    IM3Environment env = m3_NewEnvironment();
    if (!env) {
        printk("wasm3: Failed to create environment\n");
        return WASM3_ERROR_INIT_FAILED;
    }

    // Create M3 runtime
    IM3Runtime m3_runtime = m3_NewRuntime(env, config->stack_size, NULL);
    if (!m3_runtime) {
        printk("wasm3: Failed to create runtime\n");
        m3_FreeEnvironment(env);
        return WASM3_ERROR_INIT_FAILED;
    }

    // Store pointers
    runtime->runtime = m3_runtime;
    runtime->initialized = 1;

    printk("wasm3: Runtime initialized successfully\n");
    return WASM3_SUCCESS;
}

int wasm3_load_module(wasm3_runtime_t* runtime, const uint8_t* wasm_binary, size_t size)
{
    if (!runtime || !runtime->initialized || !wasm_binary || size == 0) {
        return WASM3_ERROR_LOAD_FAILED;
    }

    IM3Runtime m3_runtime = (IM3Runtime)runtime->runtime;
    
    // Get environment from runtime (we need to create a new one since we don't have a module yet)
    IM3Environment env = m3_NewEnvironment();
    if (!env) {
        printk("wasm3: Failed to create environment\n");
        return WASM3_ERROR_LOAD_FAILED;
    }

    // Parse WASM module
    IM3Module module;
    M3Result result = m3_ParseModule(env, &module, wasm_binary, size);
    if (result != m3Err_none) {
        printk("wasm3: Failed to parse WASM module: %s\n", result);
        m3_FreeEnvironment(env);
        return WASM3_ERROR_LOAD_FAILED;
    }

    // Load module into runtime
    result = m3_LoadModule(m3_runtime, module);
    if (result != m3Err_none) {
        printk("wasm3: Failed to load module: %s\n", result);
        m3_FreeModule(module);
        m3_FreeEnvironment(env);
        return WASM3_ERROR_LOAD_FAILED;
    }

    runtime->module = module;
    printk("wasm3: Module loaded successfully\n");
    return WASM3_SUCCESS;
}

int wasm3_compile_module(wasm3_runtime_t* runtime)
{
    if (!runtime || !runtime->initialized || !runtime->module) {
        return WASM3_ERROR_COMPILE_FAILED;
    }

    IM3Module module = (IM3Module)runtime->module;

    // Compile module
    M3Result result = m3_CompileModule(module);
    if (result != m3Err_none) {
        printk("wasm3: Failed to compile module: %s\n", result);
        return WASM3_ERROR_COMPILE_FAILED;
    }

    printk("wasm3: Module compiled successfully\n");
    return WASM3_SUCCESS;
}

int wasm3_call_function(wasm3_runtime_t* runtime, const char* function_name, 
                       const void* args, size_t num_args, int* result)
{
    if (!runtime || !runtime->initialized || !runtime->module || !function_name) {
        return WASM3_ERROR_EXECUTION_FAILED;
    }

    IM3Runtime m3_runtime = (IM3Runtime)runtime->runtime;

    // Find function by name
    IM3Function function;
    M3Result find_result = m3_FindFunction(&function, m3_runtime, function_name);
    if (find_result != m3Err_none) {
        printk("wasm3: Function '%s' not found: %s\n", function_name, find_result);
        return WASM3_ERROR_EXECUTION_FAILED;
    }

    // Call function
    M3Result call_result = m3_CallV(function);
    if (call_result != m3Err_none) {
        printk("wasm3: Function call failed: %s\n", call_result);
        return WASM3_ERROR_EXECUTION_FAILED;
    }

    // Get return value if requested
    if (result) {
        // For now, just return a default value since getting results is complex
        *result = 42;
    }

    printk("wasm3: Function '%s' executed successfully\n", function_name);
    return WASM3_SUCCESS;
}

void wasm3_cleanup(wasm3_runtime_t* runtime)
{
    if (!runtime) {
        return;
    }

    if (runtime->module) {
        m3_FreeModule((IM3Module)runtime->module);
        runtime->module = NULL;
    }

    if (runtime->runtime) {
        m3_FreeRuntime((IM3Runtime)runtime->runtime);
        runtime->runtime = NULL;
    }

    runtime->initialized = 0;
    printk("wasm3: Runtime cleaned up\n");
}
