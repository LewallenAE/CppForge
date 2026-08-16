cmake_minimum_required(VERSION 3.20)

project(TaskForge VERSION 1.0.0 LANGUAGES CXX)

option(TASKFORGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(TASKFORGE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(TASKFORGE_ENABLE_SANITIZERS AND TASKFORGE_ENABLE_TSAN)
    message(FATAL_ERROR "ASan/UBSan and TSan must use separate build directories")
endif()

find_package(Threads REQUIRED)

add_library(taskforge_build_options INTERFACE)
target_compile_features(taskforge_build_options INTERFACE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(taskforge_build_options INTERFACE
        -Wall
        -Wextra
        -Wconversion
        -Wsign-conversion
        -Werror
        -pedantic-errors
    )

    if(TASKFORGE_ENABLE_SANITIZERS)
        target_compile_options(taskforge_build_options INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(taskforge_build_options INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    endif()

    if(TASKFORGE_ENABLE_TSAN)
        target_compile_options(taskforge_build_options INTERFACE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
        target_link_options(taskforge_build_options INTERFACE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
    endif()
elseif(TASKFORGE_ENABLE_SANITIZERS OR TASKFORGE_ENABLE_TSAN)
    message(FATAL_ERROR "The requested sanitizer configuration requires GCC or Clang")
endif()

add_library(taskforge_core
    src/cli.cpp
    src/job.cpp
    src/task_executor.cpp
)
target_include_directories(taskforge_core PUBLIC include)
target_link_libraries(taskforge_core PUBLIC taskforge_build_options Threads::Threads)

add_executable(taskforge src/main.cpp)
target_link_libraries(taskforge PRIVATE taskforge_core)
set_target_properties(taskforge_core taskforge PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
    add_executable(taskforge_tests tests/taskforge_tests.cpp)
    target_link_libraries(taskforge_tests PRIVATE taskforge_core)
    set_target_properties(taskforge_tests PROPERTIES CXX_EXTENSIONS OFF)

    add_test(NAME taskforge_unit_and_stress COMMAND taskforge_tests)
    set_tests_properties(taskforge_unit_and_stress PROPERTIES TIMEOUT 90)

    add_test(
        NAME taskforge_integration
        COMMAND ${CMAKE_COMMAND}
            -DTASKFORGE_EXECUTABLE=$<TARGET_FILE:taskforge>
            -DTASKFORGE_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/integration-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.cmake
    )
    set_tests_properties(taskforge_integration PROPERTIES TIMEOUT 30)
endif()
