(module
  ;; Import memory
  (import "env" "memory" (memory 1))
  
  ;; Function that returns 99
  (func $get_number (result i32)
    i32.const 99)
  
  ;; Function that adds two numbers
  (func $add (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)
  
  ;; Function that multiplies two numbers
  (func $multiply (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.mul)
  
  ;; Export all functions
  (export "get_number" (func $get_number))
  (export "add" (func $add))
  (export "multiply" (func $multiply)))
