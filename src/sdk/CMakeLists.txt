# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(CMakePackageConfigHelpers)

find_pkg(Qt6 COMPONENTS Core Network Xml Concurrent Sql Svg Widgets REQUIRED)
find_pkg(OpenSSL COMPONENTS Crypto SSL REQUIRED)
find_pkg(Protobuf REQUIRED)
find_pkg(Poco COMPONENTS XML Zip)
find_pkg(Boost 1.62.0 COMPONENTS system thread date_time QUIET)
find_pkg("SndFile;LibSndFile;sndfile" REQUIRED)
find_pkg("opus;Opus" REQUIRED)
find_pkg(speexdsp REQUIRED)
find_pkg(BZip2 REQUIRED)
find_pkg(Ice
	COMPONENTS
		Ice
		IceSSL
	OPTIONAL_COMPONENTS
		IceDiscovery
		IceLocatorDiscovery
		IceUtil
	REQUIRED
)

function(mumble_sdk_copy_interface_link_libraries from_target to_target)
	get_target_property(_link_libraries ${from_target} INTERFACE_LINK_LIBRARIES)
	if(_link_libraries AND NOT _link_libraries STREQUAL "_link_libraries-NOTFOUND")
		set_property(TARGET ${to_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${_link_libraries}")
	endif()

	get_target_property(_link_options ${from_target} INTERFACE_LINK_OPTIONS)
	if(_link_options AND NOT _link_options STREQUAL "_link_options-NOTFOUND")
		set_property(TARGET ${to_target} APPEND PROPERTY INTERFACE_LINK_OPTIONS "${_link_options}")
	endif()
endfunction()

function(mumble_sdk_remove_interface_link_libraries target_name)
	get_target_property(_link_libraries ${target_name} INTERFACE_LINK_LIBRARIES)
	if(NOT _link_libraries OR _link_libraries STREQUAL "_link_libraries-NOTFOUND")
		return()
	endif()

	list(REMOVE_ITEM _link_libraries ${ARGN})
	set_target_properties(${target_name} PROPERTIES INTERFACE_LINK_LIBRARIES "${_link_libraries}")
endfunction()

function(mumble_sdk_install_target_if_possible target_name)
	if(NOT TARGET ${target_name})
		return()
	endif()

	get_target_property(_imported ${target_name} IMPORTED)
	if(_imported)
		return()
	endif()

	install(TARGETS ${target_name}
		EXPORT MumbleSDKTargets
		ARCHIVE DESTINATION "${MUMBLE_INSTALL_LIBDIR}"
		LIBRARY DESTINATION "${MUMBLE_INSTALL_LIBDIR}"
		RUNTIME DESTINATION "${MUMBLE_INSTALL_BINDIR}"
	)
endfunction()

function(mumble_sdk_install_target_with_headers_if_possible target_name)
	if(NOT TARGET ${target_name})
		return()
	endif()

	get_target_property(_imported ${target_name} IMPORTED)
	if(_imported)
		return()
	endif()

	install(TARGETS ${target_name}
		EXPORT MumbleSDKTargets
		ARCHIVE DESTINATION "${MUMBLE_INSTALL_LIBDIR}"
		LIBRARY DESTINATION "${MUMBLE_INSTALL_LIBDIR}"
		RUNTIME DESTINATION "${MUMBLE_INSTALL_EXECUTABLEDIR}"
		FILE_SET headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
	)
endfunction()

add_library(mumble_sdk_common STATIC
	"Common.cpp"
)

if(WIN32 AND NOT bundled-speex)
	target_sources(mumble_sdk_common PRIVATE "SpeexCompat.cpp")
endif()

target_include_directories(mumble_sdk_common
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE
		"${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(mumble_sdk_common
	PUBLIC
		shared
		Qt6::Widgets
)

if(sdk-client)
	add_library(mumble_sdk_client STATIC
		"Client.cpp"
	)

	target_link_libraries(mumble_sdk_client
		PRIVATE
			nlohmann_json::nlohmann_json
		PUBLIC
			mumble_client_object_lib
			mumble_sdk_common
	)

	target_include_directories(mumble_sdk_client
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
			$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		PRIVATE
			"${CMAKE_SOURCE_DIR}/src"
			"${CMAKE_SOURCE_DIR}/src/mumble"
			"${CMAKE_SOURCE_DIR}/src/mumble/widgets"
	)

	add_library(MumbleSDK::client ALIAS mumble_sdk_client)
	set_target_properties(mumble_sdk_client PROPERTIES EXPORT_NAME client)
endif()

if(sdk-server)
	add_library(mumble_sdk_server STATIC
		"Server.cpp"
	)

	target_link_libraries(mumble_sdk_server
		PRIVATE
			nlohmann_json::nlohmann_json
		PUBLIC
			mumble_server_object_lib
			mumble_sdk_common
	)

	target_include_directories(mumble_sdk_server
		PUBLIC
			$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
			$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		PRIVATE
			"${CMAKE_SOURCE_DIR}/src"
			"${CMAKE_SOURCE_DIR}/src/murmur"
	)

	add_library(MumbleSDK::server ALIAS mumble_sdk_server)
	set_target_properties(mumble_sdk_server PROPERTIES EXPORT_NAME server)
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/mumble-sdk"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

mumble_sdk_install_target_if_possible(mumble_sdk_common)
mumble_sdk_install_target_if_possible(shared)
mumble_sdk_install_target_if_possible(mumble_database)
mumble_sdk_install_target_if_possible(mumble_server_database)
mumble_sdk_install_target_if_possible(nlohmann_json)
mumble_sdk_install_target_if_possible(soci_interface)
mumble_sdk_install_target_if_possible(mumble_client_object_lib)
mumble_sdk_install_target_if_possible(mumble_server_object_lib)
mumble_sdk_install_target_if_possible(utfcpp)
mumble_sdk_install_target_if_possible(smallft)
mumble_sdk_install_target_if_possible(SPSCQueue)
mumble_sdk_install_target_if_possible(xinputcheck)
mumble_sdk_install_target_if_possible(speexdsp)
mumble_sdk_install_target_if_possible(rnnoise)
mumble_sdk_install_target_if_possible(mumble_sdk_client)
mumble_sdk_install_target_if_possible(mumble_sdk_server)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/MumbleSDKConfigVersion.cmake"
	COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
	"${CMAKE_SOURCE_DIR}/cmake/MumbleSDKConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/MumbleSDKConfig.cmake"
	INSTALL_DESTINATION "${MUMBLE_INSTALL_LIBDIR}/cmake/MumbleSDK"
)

install(EXPORT MumbleSDKTargets
	FILE MumbleSDKTargets.cmake
	NAMESPACE MumbleSDK::
	DESTINATION "${MUMBLE_INSTALL_LIBDIR}/cmake/MumbleSDK"
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/MumbleSDKConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/MumbleSDKConfigVersion.cmake"
	DESTINATION "${MUMBLE_INSTALL_LIBDIR}/cmake/MumbleSDK"
)

if(TARGET TracyClient)
	install(FILES $<TARGET_FILE:TracyClient> DESTINATION "${MUMBLE_INSTALL_LIBDIR}")
endif()

foreach(_soci_target soci_core soci_mysql soci_postgresql soci_sqlite3)
	if(TARGET ${_soci_target})
		install(FILES $<TARGET_FILE:${_soci_target}> DESTINATION "${MUMBLE_INSTALL_LIBDIR}")
	endif()
endforeach()
