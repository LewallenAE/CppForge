cmake_minimum_required(VERSION 3.20)

project(LogForge VERSION 1.0.0 LANGUAGES CXX)

option(LOGFORGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(logforge_warnings INTERFACE)
target_compile_features(logforge_warnings INTERFACE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(logforge_warnings INTERFACE
        -Wall
        -Wextra
        -Wconversion
        -Wsign-conversion
        -Werror
        -pedantic-errors
    )
endif()

if(LOGFORGE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(logforge_warnings INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(logforge_warnings INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    else()
        message(FATAL_ERROR "LOGFORGE_ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

add_library(logforge_core
    src/analyzer.cpp
    src/options.cpp
    src/parser.cpp
)
target_include_directories(logforge_core PUBLIC include)
target_link_libraries(logforge_core PUBLIC logforge_warnings)

add_executable(logforge src/main.cpp)
target_link_libraries(logforge PRIVATE logforge_core)
set_target_properties(logforge_core logforge PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
    add_executable(logforge_tests tests/logforge_tests.cpp)
    target_link_libraries(logforge_tests PRIVATE logforge_core)
    set_target_properties(logforge_tests PROPERTIES CXX_EXTENSIONS OFF)

    add_test(NAME logforge_unit COMMAND logforge_tests)
    add_test(
        NAME logforge_integration
        COMMAND ${CMAKE_COMMAND}
            -DLOGFORGE_EXECUTABLE=$<TARGET_FILE:logforge>
            -DLOGFORGE_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/integration-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.cmake
    )
endif()
