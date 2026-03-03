add_executable(smp_tests
    test_main.c
    test_support.c
)

target_include_directories(smp_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(smp_tests PRIVATE smp tinycbor)

add_test(NAME smp_tests COMMAND smp_tests)
