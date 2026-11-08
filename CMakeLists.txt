# SPDX-FileCopyrightText: 2026 KAS
#
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)

#====================================

set(DIRNAME KAS)
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/source/${DIRNAME})
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/${DIRNAME})
set(PUBLIC_DEFINED_LIST)
set(PRIVATE_DEFINED_LIST)

#====================================

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

option(ENABLE_AUTH_SERVICES "Enable AUTH_SERVICES" ON)
option(ENABLE_STREAM "Enable STREAM" ON)
option(ENABLE_CONVERTED "Enable CONVERTED" ON)
option(ENABLE_THREAD "Enable THREAD" ON)
option(ENABLE_NET "Enable NET" ON)
option(ENABLE_SERIAL "Enable SERIAL" ON)
option(ENABLE_JSON "Enable JSON" ON)

set(KASLIBCXX_STANDART "20261018" CACHE STRING "KASLIBCXX standard version identifier")

option(ENABLE_TESTS "Enable tests" OFF)

#====================================

list(APPEND PUBLIC_DEFINED_LIST
	"KASLIBCXX_STANDART=${KASLIBCXX_STANDART}"
)

#====================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
	file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" KASLIB_VERSION)
	string(STRIP "${KASLIB_VERSION}" KASLIB_VERSION)
	message(STATUS "KASLIB_VERSION from file: ${KASLIB_VERSION}")
else()
	message(FATAL_ERROR "File 'version.txt' not found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

#------------------------------------

if(KASLIB_VERSION)
	project(KAS VERSION ${KASLIB_VERSION} LANGUAGES CXX)
else()
	message(FATAL_ERROR "Not DEFINE 'KASLIB_VERSION'!")
endif()

#====================================

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

#------------------------------------

set(SOURCES)

#====================================
message(STATUS "USE KASLIBCXX_STANDART: ${KASLIBCXX_STANDART}")
message(STATUS "USE ENABLE_TESTS: ${ENABLE_TESTS}")
#------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
#------------------------------------
add_library(${PROJECT_NAME})
message(STATUS "-----------------------------")

#====================================
macro(install_headers dir)
	if(ENABLE_TESTS)
		install(DIRECTORY "${INCLUDE_DIR}/${dir}"
			DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${DIRNAME})
	else()
		install(DIRECTORY "${INCLUDE_DIR}/${dir}"
			DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${DIRNAME}
			PATTERN "tests.h" EXCLUDE
			PATTERN "test_*.h" EXCLUDE)
	endif()
endmacro()

#====================================

if(ENABLE_THREAD STREQUAL "ON")
	# THREAD
	message(STATUS "USE ENABLE_THREAD")

	list(APPEND PRIVATE_DEFINED_LIST "ENABLE_THREAD=1")

	list(APPEND SOURCES
			"${SOURCE_DIR}/thread/Thread.cpp"
			"${SOURCE_DIR}/thread/JThread.cpp"
			"${SOURCE_DIR}/thread/ThreadPool.cpp"
		)
	install_headers(thread)

endif()

#------------------------------------

if(ENABLE_NET)
	# ENABLE_NET
	message(STATUS "USE ENABLE_NET")
	list(APPEND PRIVATE_DEFINED_LIST "ENABLE_NET=1")

	list(APPEND SOURCES
		"${SOURCE_DIR}/net/general.cpp"
		"${SOURCE_DIR}/net/socket/SocketScoped.cpp"
		"${SOURCE_DIR}/net/socket/SocketClient.cpp"
		"${SOURCE_DIR}/net/socket/SocketServer.cpp"
		"${SOURCE_DIR}/net/socket/SocketAddress.cpp"
		"${SOURCE_DIR}/net/socket/ClientConnection.cpp"
		"${SOURCE_DIR}/net/http/Request.cpp"
		"${SOURCE_DIR}/net/http/Response.cpp"
		"${SOURCE_DIR}/net/http/HttpRoute.cpp"
		"${SOURCE_DIR}/net/http/HttpServer.cpp"
		"${SOURCE_DIR}/net/http/HttpRange.cpp"
		"${SOURCE_DIR}/net/http/websocket/WebSocketConnection.cpp"
		"${SOURCE_DIR}/net/http/websocket/WebSocketFrame.cpp"
		"${SOURCE_DIR}/net/http/websocket/WebSocketHandshake.cpp"
	)
	install_headers(net)
endif()

#------------------------------------

if(ENABLE_SERIAL)
	# ENABLE_SERIAL
	message(STATUS "USE ENABLE_SERIAL")

	list(APPEND PRIVATE_DEFINED_LIST "ENABLE_SERIAL=1")
	list(APPEND SOURCES
		"${SOURCE_DIR}/serial/FileHandleScoped.cpp"
		"${SOURCE_DIR}/serial/Serial.cpp"
	)
	install_headers(serial)
endif()

#------------------------------------

if(ENABLE_CONVERTED)
	# ENABLE_CONVERTED
	message(STATUS "USE ENABLE_CONVERTED")
	list(APPEND PRIVATE_DEFINED_LIST "ENABLE_CONVERTED=1")
	list(APPEND SOURCES
		"${SOURCE_DIR}/convert/md5/v3/md5.cpp"
		"${SOURCE_DIR}/convert/base64/v1/base64.cpp"
		"${SOURCE_DIR}/convert/sha256/v2/sha256.cpp"
		"${SOURCE_DIR}/convert/sha1/v1/sha1.cpp"
		"${SOURCE_DIR}/convert/jwt/v2/jwt.cpp"
		"${SOURCE_DIR}/convert/crc/ccitt_false/crc16.cpp"
		"${SOURCE_DIR}/convert/serialize/RawPacket/RawPacket.cpp"
	)
	install_headers(convert)
endif()

#------------------------------------

if(ENABLE_STREAM STREQUAL "ON")
	# STREAM
	message(STATUS "USE ENABLE_STREAM")
	list(APPEND PRIVATE_DEFINED_LIST "ENABLE_STREAM=1")
	list(APPEND SOURCES "${SOURCE_DIR}/stream/args/InputArgs/InputArgs.cpp")
	list(APPEND SOURCES "${SOURCE_DIR}/stream/args/ArgsHandler/ArgsHandler.cpp")
	list(APPEND SOURCES "${SOURCE_DIR}/stream/args/ArgsHandler/CommandInfo.cpp")
	list(APPEND SOURCES "${SOURCE_DIR}/stream/cots/v3/ColorOutToStream.cpp")

	install_headers(stream)
endif()

#====================================

if(ENABLE_JSON STREQUAL "ON")
	message(STATUS "USE ENABLE_JSON")
	install_headers(json)
endif()

	# ALWAYS_USED
	message(STATUS "ALWAYS_USED:")

	message(STATUS "USE AUTH_SERVICES")
	install_headers(auth_services)

	message(STATUS "USE CONCEPTS")
	install_headers(concepts)

	message(STATUS "USE UTILS")
	list(APPEND SOURCES
		"${SOURCE_DIR}/utils/v5/utils.cpp"
	)
	install_headers(utils)

	message(STATUS "USE FUNCTIONAL")
	install_headers(functional)

	message(STATUS "USE ENABLE_EXPECTED")
	install_headers(expected)

#====================================

if(NOT "${SOURCES}" STREQUAL "")

	message("--------------------------------")
	message("SOURCE FILES")
	foreach(source ${SOURCES})
		message(STATUS ${source})
	endforeach()
	message("--------------------------------")
endif()

#====================================

if("${SOURCES}" STREQUAL "")
	message(FATAL_ERROR "Files NOT FOUND!")
endif()

if(BUILD_SHARED_LIBS)
	message(STATUS "Build type: SHARED")
else()
	message(STATUS "Build type: STATIC")
endif()

#====================================
target_precompile_headers(${PROJECT_NAME} PRIVATE
	<algorithm>
	<array>
	<atomic>
	<chrono>
	<cstdint>
	<filesystem>
	<fstream>
	<memory>
	<optional>
	<span>
	<string>
	<string_view>
	<vector>
)
#------------------------------------
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
#------------------------------------
target_compile_definitions(	${PROJECT_NAME}
	PRIVATE
		${PRIVATE_DEFINED_LIST}
	PUBLIC
		${PUBLIC_DEFINED_LIST}
)
#------------------------------------
if(NOT BUILD_SHARED_LIBS)
set(KAS_LTO_OPTIONS $<$<CONFIG:Release>:-flto>)
else()
set(KAS_LTO_OPTIONS "")
endif()
#------------------------------------
target_compile_options(${PROJECT_NAME} PRIVATE
	-Wall
	-Wextra
	-Wpedantic
	-Wconversion
	$<$<CONFIG:Release>:-O3>
	${KAS_LTO_OPTIONS}
	-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=
)
#------------------------------------
target_link_options(${PROJECT_NAME} PRIVATE
	$<$<CONFIG:Release>:-O3>
	${KAS_LTO_OPTIONS})
#------------------------------------
target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
#------------------------------------
set_target_properties(
	${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 20
	CXX_EXTENSIONS OFF
	CXX_STANDARD_REQUIRED ON
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)
#====================================
add_library(KASLIB::KASLIB ALIAS ${PROJECT_NAME})
#====================================

# --- Bluetooth (optional) ---
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(BLUEZ QUIET bluez)
endif()

if(BLUEZ_FOUND)
	target_compile_definitions(${PROJECT_NAME} PRIVATE KASLIBCXX_NET_HAS_BLUETOOTH=1)
	target_include_directories(${PROJECT_NAME} PRIVATE ${BLUEZ_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} PRIVATE ${BLUEZ_LIBRARIES})
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
	set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

#====================================

if(ENABLE_TESTS STREQUAL "ON")
	enable_testing()
	add_subdirectory(tests)
endif()

#====================================

INSTALL(
	TARGETS ${PROJECT_NAME}
	EXPORT "${PROJECT_NAME}_targets"
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
	EXPORT ${PROJECT_NAME}_targets
	FILE ${PROJECT_NAME}_targets.cmake
	NAMESPACE KASLIB::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

#====================================

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

#====================================
