#ifndef WASM_TEST_MODULE_H
#define WASM_TEST_MODULE_H

// Simple test WASM bytecode for Phase 1
// This is a minimal WASM module that just returns a constant
// In Phase 2, we'll load real WASM bytecode via BLE

// Minimal WASM module (version 1, empty sections)
static const uint8_t test_wasm_module[] = {
    0x00, 0x61, 0x73, 0x6d,  // Magic number: "\0asm"
    0x01, 0x00, 0x00, 0x00,  // Version: 1
    0x00,                      // Custom section count
    0x00,                      // Type section count
    0x00,                      // Import section count
    0x00,                      // Function section count
    0x00,                      // Table section count
    0x00,                      // Memory section count
    0x00,                      // Global section count
    0x00,                      // Export section count
    0x00,                      // Start section count
    0x00,                      // Element section count
    0x00,                      // Code section count
    0x00                       // Data section count
};

#define TEST_WASM_MODULE_SIZE sizeof(test_wasm_module)

#endif // WASM_TEST_MODULE_H
