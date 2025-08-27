cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my5340_app)

target_sources(app PRIVATE
    src/main.c
    src/wasm_interpreter.c
    src/wasm3_wrapper.c
)