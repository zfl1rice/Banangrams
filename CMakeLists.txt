cmake_minimum_required(VERSION 3.24)
project(bananagrams VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BANANAGRAMS_ENABLE_ASAN "Enable AddressSanitizer when supported" OFF)
option(BANANAGRAMS_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer when supported" OFF)

# Keep project warnings out of third-party dependency builds.
add_library(bananagrams_options INTERFACE)
if(MSVC)
    target_compile_options(bananagrams_options INTERFACE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bananagrams_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# Probe compilation and linking: a compiler may lack sanitizer runtimes.
include(CheckCXXSourceCompiles)
if(BANANAGRAMS_ENABLE_ASAN OR BANANAGRAMS_ENABLE_UBSAN)
    if(MSVC)
        if(BANANAGRAMS_ENABLE_UBSAN)
            message(FATAL_ERROR "UBSan is unavailable with this MSVC-style toolchain.")
        endif()
        # MSVC runtime checks and incremental linking conflict with ASan.
        string(REGEX REPLACE "/RTC[1su]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        set(sanitizer_compile_flags /fsanitize=address)
        set(sanitizer_link_flags /INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(BANANAGRAMS_ENABLE_ASAN)
            list(APPEND sanitizer_compile_flags -fsanitize=address)
            list(APPEND sanitizer_link_flags -fsanitize=address)
        endif()
        if(BANANAGRAMS_ENABLE_UBSAN)
            list(APPEND sanitizer_compile_flags -fsanitize=undefined)
            list(APPEND sanitizer_link_flags -fsanitize=undefined)
        endif()
        list(APPEND sanitizer_compile_flags -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR "Sanitizers are not configured for this compiler.")
    endif()
    string(JOIN " " CMAKE_REQUIRED_FLAGS ${sanitizer_compile_flags})
    set(CMAKE_REQUIRED_LINK_OPTIONS ${sanitizer_link_flags})
    unset(BANANAGRAMS_SANITIZERS_AVAILABLE CACHE)
    check_cxx_source_compiles("int main() { return 0; }" BANANAGRAMS_SANITIZERS_AVAILABLE)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT BANANAGRAMS_SANITIZERS_AVAILABLE)
        message(FATAL_ERROR "Requested sanitizers cannot compile/link. Install their runtimes or disable the options.")
    endif()
    target_compile_options(bananagrams_options INTERFACE ${sanitizer_compile_flags})
    target_link_options(bananagrams_options INTERFACE ${sanitizer_link_flags})
endif()

# Every gameplay stub is compiled into this reusable native library.
add_library(bananagrams_engine STATIC
    src/board.cpp
    src/rack.cpp
    src/tile_pool.cpp
    src/lexicon.cpp
    src/board_validator.cpp
    src/game.cpp)
add_library(bananagrams::engine ALIAS bananagrams_engine)
target_include_directories(bananagrams_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(bananagrams_engine PUBLIC cxx_std_20)
target_link_libraries(bananagrams_engine PRIVATE bananagrams_options)

add_executable(bananagrams_cli apps/cli/main.cpp)
target_link_libraries(bananagrams_cli PRIVATE bananagrams::engine bananagrams_options)

if(EMSCRIPTEN)
    add_subdirectory(bindings/wasm)
endif()

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    # doctest is the only dependency: version and archive digest are pinned.
    FetchContent_Declare(doctest
        URL https://codeload.github.com/doctest/doctest/tar.gz/refs/tags/v2.4.12
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)

    # Include the existing component suites as well as the game suites below.
    foreach(component IN ITEMS board rack lexicon tile_pool)
        add_executable(bananagrams_${component}_tests tests/${component}_test.cpp)
        target_link_libraries(bananagrams_${component}_tests PRIVATE
            bananagrams::engine bananagrams_options doctest::doctest)
        add_test(NAME ${component} COMMAND bananagrams_${component}_tests)
        set_tests_properties(${component} PROPERTIES TIMEOUT 30)
    endforeach()

    add_executable(bananagrams_tests tests/smoke_test.cpp)
    target_link_libraries(bananagrams_tests PRIVATE
        bananagrams::engine bananagrams_options doctest::doctest)
    add_test(NAME scaffold_smoke COMMAND bananagrams_tests)
    add_test(NAME cli_smoke COMMAND bananagrams_cli)
    set_tests_properties(cli_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "engine skeleton: gameplay is not implemented yet")
endif()
