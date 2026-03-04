cmake_minimum_required(VERSION 3.16)
project(smp_protocol C)

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

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BUILD_EXAMPLES)
    add_executable(minimal_target_example examples/minimal_target.c)
    target_link_libraries(minimal_target_example PRIVATE smp)

    add_executable(complete_upload_demo examples/complete_upload_demo.c)
    target_link_libraries(complete_upload_demo PRIVATE smp)
    target_include_directories(complete_upload_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/tinycbor-src/src
        ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/tinycbor-build
    )

    add_executable(error_scenarios_demo examples/error_scenarios_demo.c)
    target_link_libraries(error_scenarios_demo PRIVATE smp)
    target_include_directories(error_scenarios_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/tinycbor-src/src
        ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/tinycbor-build
    )

    add_executable(integration_target_example examples/integration_target.c)
    target_link_libraries(integration_target_example PRIVATE smp)
endif()
