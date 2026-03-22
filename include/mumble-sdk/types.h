// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SDK_TYPES_H_
#define MUMBLE_SDK_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mumble_sdk_runtime mumble_sdk_runtime_t;
typedef struct mumble_sdk_client mumble_sdk_client_t;
typedef struct mumble_sdk_server mumble_sdk_server_t;

typedef enum mumble_sdk_status {
	MUMBLE_SDK_STATUS_OK = 0,
	MUMBLE_SDK_STATUS_INVALID_ARGUMENT = 1,
	MUMBLE_SDK_STATUS_ALREADY_EXISTS = 2,
	MUMBLE_SDK_STATUS_NOT_FOUND = 3,
	MUMBLE_SDK_STATUS_BUSY = 4,
	MUMBLE_SDK_STATUS_UNSUPPORTED = 5,
	MUMBLE_SDK_STATUS_INTERNAL_ERROR = 6
} mumble_sdk_status_t;

typedef enum mumble_sdk_log_level {
	MUMBLE_SDK_LOG_TRACE = 0,
	MUMBLE_SDK_LOG_DEBUG = 1,
	MUMBLE_SDK_LOG_INFO = 2,
	MUMBLE_SDK_LOG_WARN = 3,
	MUMBLE_SDK_LOG_ERROR = 4,
	MUMBLE_SDK_LOG_FATAL = 5
} mumble_sdk_log_level_t;

typedef struct mumble_sdk_vec3 {
	float x;
	float y;
	float z;
} mumble_sdk_vec3_t;

typedef struct mumble_sdk_positional_frame {
	size_t size;
	mumble_sdk_vec3_t player_pos;
	mumble_sdk_vec3_t player_dir;
	mumble_sdk_vec3_t player_axis;
	mumble_sdk_vec3_t camera_pos;
	mumble_sdk_vec3_t camera_dir;
	mumble_sdk_vec3_t camera_axis;
	const char *context;
	const char *identity;
} mumble_sdk_positional_frame_t;

typedef enum mumble_sdk_client_event_type {
	MUMBLE_SDK_CLIENT_EVENT_CONNECTED = 1,
	MUMBLE_SDK_CLIENT_EVENT_SYNCHRONIZED = 2,
	MUMBLE_SDK_CLIENT_EVENT_DISCONNECTED = 3,
	MUMBLE_SDK_CLIENT_EVENT_TEXT_MESSAGE = 4,
	MUMBLE_SDK_CLIENT_EVENT_WARNING = 5,
	MUMBLE_SDK_CLIENT_EVENT_CONNECTION_ERROR = 6
} mumble_sdk_client_event_type_t;

typedef struct mumble_sdk_client_event {
	size_t size;
	mumble_sdk_client_event_type_t type;
	const char *message;
	const char *secondary_message;
	int64_t value_a;
	int64_t value_b;
} mumble_sdk_client_event_t;

typedef enum mumble_sdk_server_event_type {
	MUMBLE_SDK_SERVER_EVENT_STARTED = 1,
	MUMBLE_SDK_SERVER_EVENT_STOPPED = 2,
	MUMBLE_SDK_SERVER_EVENT_VIRTUAL_SERVER_STARTED = 3,
	MUMBLE_SDK_SERVER_EVENT_VIRTUAL_SERVER_STOPPED = 4
} mumble_sdk_server_event_type_t;

typedef struct mumble_sdk_server_event {
	size_t size;
	mumble_sdk_server_event_type_t type;
	const char *message;
	const char *secondary_message;
	int64_t value_a;
	int64_t value_b;
} mumble_sdk_server_event_t;

typedef void (*mumble_sdk_log_callback_t)(void *userdata, mumble_sdk_log_level_t level, const char *message);
typedef void (*mumble_sdk_client_event_callback_t)(void *userdata, const mumble_sdk_client_event_t *event_data);
typedef void (*mumble_sdk_server_event_callback_t)(void *userdata, const mumble_sdk_server_event_t *event_data);
typedef uint8_t (*mumble_sdk_positional_provider_t)(void *userdata, mumble_sdk_positional_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif // MUMBLE_SDK_TYPES_H_
