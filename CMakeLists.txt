cmake_minimum_required(VERSION 4.1)
project(radiance)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_compile_definitions(-DDEBUG_OUTPUT)

add_executable(radiance main.cpp external/hde/hde64.c)

target_sources(radiance PUBLIC
        FILE_SET module_files TYPE CXX_MODULES FILES
            src/cpu/cpu_affinity_scope.ixx
            src/hook/hook_base.ixx
            src/hook/impl/splicing_hook.ixx
            src/hook/impl/splicing_trampoline.ixx
            src/hook/hook_dispatcher.ixx
            src/memory/memory_page.ixx
            src/memory/memory_allocator.ixx
            src/memory/memory_stl_adapter.ixx
            src/memory/memory_metadata.ixx
            src/hook/impl/splicing_rebuilder.ixx
            src/hook/impl/splicing_patcher.ixx
)

target_compile_options(radiance PRIVATE -mcx16)

target_include_directories(radiance PUBLIC
        "."
        "src/"
        "external/"
        "external/libguarded/src/"
)