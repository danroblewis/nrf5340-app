#ifndef WASM_INTERPRETER_H
#define WASM_INTERPRETER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Simple WASM interpreter structure
typedef struct {
    uint8_t *bytecode;
    size_t bytecode_size;
    uint8_t *memory;
    size_t memory_size;
    bool is_loaded;
    bool is_running;
} wasm_interpreter_t;

// Function prototypes
int wasm_interpreter_init(wasm_interpreter_t *interpreter);
int wasm_interpreter_load_bytecode(wasm_interpreter_t *interpreter, 
                                  const uint8_t *bytecode, 
                                  size_t size);
int wasm_interpreter_execute(wasm_interpreter_t *interpreter);
void wasm_interpreter_cleanup(wasm_interpreter_t *interpreter);

// Simple test function that prints to serial
void wasm_test_function(void);

#endif // WASM_INTERPRETER_H
