# SPDX-FileCopyrightText: 2026 KAS
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)

#----------------------------------

set(KASLIB_TEST_SANITIZER "OFF" CACHE STRING
    "Sanitizer for tests: OFF, address, thread, undefined")
set_property(CACHE KASLIB_TEST_SANITIZER PROPERTY STRINGS
    OFF address thread undefined)

set(KASLIB_TEST_SANITIZER_FLAGS "")
if(KASLIB_TEST_SANITIZER STREQUAL "address")
    set(KASLIB_TEST_SANITIZER_FLAGS -fsanitize=address,undefined)
elseif(KASLIB_TEST_SANITIZER STREQUAL "thread")
    set(KASLIB_TEST_SANITIZER_FLAGS -fsanitize=thread)
elseif(KASLIB_TEST_SANITIZER STREQUAL "undefined")
    set(KASLIB_TEST_SANITIZER_FLAGS -fsanitize=undefined)
elseif(NOT KASLIB_TEST_SANITIZER STREQUAL "OFF")
    message(FATAL_ERROR
        "KASLIB_TEST_SANITIZER must be one of: OFF, address, thread, undefined")
endif()

#----------------------------------

set(KASLIB_TEST_COMMON_OPTIONS
    -Wall -Wextra -Wpedantic
    -Wno-system-headers
    ${KASLIB_TEST_SANITIZER_FLAGS}
    $<$<CONFIG:Release>:-O1>
)

set(KASLIB_TEST_COMMON_LINK_OPTIONS
    ${KASLIB_TEST_SANITIZER_FLAGS}
    $<$<CONFIG:Release>:-O1>
)

set(KASLIB_TEST_PCH
    <atomic> <chrono> <cstdint> <future> <memory>
    <stdexcept> <string> <string_view> <thread>
    <type_traits> <utility> <vector>
)

#----------------------------------

set(KASLIB_TEST_LAUNCHER "")
if(KASLIB_TEST_SANITIZER STREQUAL "thread")
    find_program(SETARCH_EXECUTABLE setarch)
    if(SETARCH_EXECUTABLE)
        set(KASLIB_TEST_LAUNCHER ${SETARCH_EXECUTABLE} ${CMAKE_SYSTEM_PROCESSOR} -R)
    else()
        message(WARNING
            "TSan is enabled but 'setarch' was not found. "
            "Tests may fail with 'ThreadSanitizer: unexpected memory mapping'.")
    endif()
endif()

#----------------------------------

function(kaslib_add_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE include)
    target_link_libraries(${name} PRIVATE KASLIB::KASLIB)
    target_compile_options(${name} PRIVATE ${KASLIB_TEST_COMMON_OPTIONS})
    target_link_options(${name} PRIVATE ${KASLIB_TEST_COMMON_LINK_OPTIONS})
    target_precompile_headers(${name} PRIVATE ${KASLIB_TEST_PCH})
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 20
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )
    if(KASLIB_TEST_LAUNCHER)
        add_test(NAME ${name}
            COMMAND ${KASLIB_TEST_LAUNCHER} $<TARGET_FILE:${name}>)
    else()
        add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}>)
    endif()
endfunction()

#----------------------------------

kaslib_add_test(test_json main_module_json.cpp)

kaslib_add_test(test_always main_module_always.cpp)

if(ENABLE_CONVERTED)
    kaslib_add_test(test_convert main_module_converted.cpp)
endif()
if(ENABLE_THREAD)
    kaslib_add_test(test_thread main_module_thread.cpp)
endif()
if(ENABLE_NET)
    kaslib_add_test(test_net main_module_net.cpp)
endif()
if(ENABLE_SERIAL)
    kaslib_add_test(test_serial main_module_serial.cpp)
endif()
if(ENABLE_STREAM)
    kaslib_add_test(test_stream main_module_stream.cpp)
endif()