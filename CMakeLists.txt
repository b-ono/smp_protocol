cmake_minimum_required(VERSION 3.16)
project(sem_protocol C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)

set(WITH_CBOR2JSON OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    tinycbor
    GIT_REPOSITORY https://github.com/intel/tinycbor.git
    GIT_TAG 6442e749ca811e24afad1551338a45c75e09808f
)
FetchContent_MakeAvailable(tinycbor)

add_library(smp
    src/smp_core.c
    src/smp_dispatch.c
    src/smp_cmd_img_upload.c
    src/smp_cbor_adapter.c
)

target_include_directories(smp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(smp PRIVATE tinycbor)

enable_testing()
add_subdirectory(tests)

add_executable(minimal_target_example examples/minimal_target.c)
target_link_libraries(minimal_target_example PRIVATE smp)
