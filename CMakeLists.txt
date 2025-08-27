cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my5340_app)

# Set wasm3 build options to avoid problematic dependencies
set(BUILD_WASI "none" CACHE STRING "WASI implementation" FORCE)
set(BUILD_NATIVE OFF CACHE BOOL "Build with machine-specific optimisations" FORCE)

# Ensure wasm3 is built with the cross-compiler
set(CMAKE_C_COMPILER ${CMAKE_C_COMPILER} CACHE STRING "C compiler" FORCE)
set(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} CACHE STRING "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER ${CMAKE_ASM_COMPILER} CACHE STRING "ASM compiler" FORCE)

# Create our app target
target_sources(app PRIVATE
    src/main.c
    src/wasm3_wrapper.c
)

# Include wasm3 headers
target_include_directories(app PRIVATE $ENV{NCS_ROOT}/modules/lib/wasm3/source)

# Build wasm3 sources directly as part of our project
target_sources(app PRIVATE
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_api_libc.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_bind.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_code.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_compile.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_core.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_env.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_exec.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_function.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_info.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_module.c
    $ENV{NCS_ROOT}/modules/lib/wasm3/source/m3_parse.c
)

# Add wasm3-specific compile definitions
target_compile_definitions(app PRIVATE
    d_m3HasTracer
    d_m3HasWASI
)

