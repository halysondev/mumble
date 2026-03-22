cmake_minimum_required(VERSION 3.23)

project(mumble_sdk_consumer LANGUAGES C CXX)

find_package(MumbleSDK CONFIG REQUIRED)

option(MUMBLE_SDK_BUILD_SERVER_SMOKE "Build the standalone server SDK smoke executable." OFF)

add_executable(mumble_sdk_smoke main.cpp)
target_link_libraries(mumble_sdk_smoke PRIVATE MumbleSDK::client)

if(MUMBLE_SDK_BUILD_SERVER_SMOKE)
	add_executable(mumble_sdk_server_smoke server_main.cpp)
	target_link_libraries(mumble_sdk_server_smoke PRIVATE MumbleSDK::server)
endif()

if(MSVC)
	target_compile_options(mumble_sdk_smoke PRIVATE /W4)
else()
	target_compile_options(mumble_sdk_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()
