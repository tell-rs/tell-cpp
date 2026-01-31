cmake_minimum_required(VERSION 3.14)
project(tell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Library ---
add_library(tell
    src/config.cpp
    src/client.cpp
    src/transport.cpp
    src/worker.cpp
)

target_include_directories(tell
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(tell PRIVATE Threads::Threads)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(tell PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests (optional) ---
option(TELL_BUILD_TESTS "Build tests" OFF)
if(TELL_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(tell_config_test     tests/config_test.cpp)
        add_executable(tell_validation_test tests/validation_test.cpp)
        add_executable(tell_encoding_test   tests/encoding_test.cpp)
        add_executable(tell_props_test      tests/props_test.cpp)
        add_executable(tell_client_test     tests/client_test.cpp)

        foreach(test_target tell_config_test tell_validation_test tell_encoding_test tell_props_test tell_client_test)
            target_link_libraries(${test_target} PRIVATE tell GTest::gtest GTest::gtest_main Threads::Threads)
            target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
            add_test(NAME ${test_target} COMMAND ${test_target})
        endforeach()
    else()
        message(STATUS "GTest not found, tests disabled")
    endif()
endif()

# --- Benchmarks (optional) ---
option(TELL_BUILD_BENCHMARKS "Build benchmarks" OFF)
if(TELL_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(tell_encoding_bench  bench/encoding_bench.cpp)
    add_executable(tell_hot_path_bench  bench/hot_path_bench.cpp)
    add_executable(tell_pipeline_bench  bench/pipeline_bench.cpp)

    foreach(bench_target tell_encoding_bench tell_hot_path_bench tell_pipeline_bench)
        target_link_libraries(${bench_target} PRIVATE tell benchmark::benchmark Threads::Threads)
        target_include_directories(${bench_target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/bench
        )
    endforeach()
endif()

# --- Examples (optional) ---
option(TELL_BUILD_EXAMPLES "Build examples" OFF)
if(TELL_BUILD_EXAMPLES)
    add_executable(tell_events  examples/events.cpp)
    add_executable(tell_config  examples/config.cpp)
    add_executable(tell_logging examples/logging.cpp)
    add_executable(tell_e2e     examples/e2e.cpp)

    foreach(example_target tell_events tell_config tell_logging tell_e2e)
        target_link_libraries(${example_target} PRIVATE tell Threads::Threads)
    endforeach()
endif()

# --- Install ---
install(TARGETS tell EXPORT tell-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/tell DESTINATION include)
install(EXPORT tell-targets
    FILE tell-config.cmake
    NAMESPACE tell::
    DESTINATION lib/cmake/tell
)
