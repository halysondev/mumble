// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SDK_SERVER_H_
#define MUMBLE_SDK_SERVER_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

mumble_sdk_status_t mumble_sdk_server_create(mumble_sdk_runtime_t *runtime, mumble_sdk_server_t **out_server);
void mumble_sdk_server_destroy(mumble_sdk_server_t *server);

mumble_sdk_status_t mumble_sdk_server_set_event_callback(mumble_sdk_server_t *server,
														 mumble_sdk_server_event_callback_t callback, void *userdata);
mumble_sdk_status_t mumble_sdk_server_set_ini_file(mumble_sdk_server_t *server, const char *ini_file_path);
mumble_sdk_status_t mumble_sdk_server_set_config(mumble_sdk_server_t *server, const char *key, const char *value);
mumble_sdk_status_t mumble_sdk_server_get_config(mumble_sdk_server_t *server, const char *key, char **out_value);

mumble_sdk_status_t mumble_sdk_server_start(mumble_sdk_server_t *server);
mumble_sdk_status_t mumble_sdk_server_stop(mumble_sdk_server_t *server);

mumble_sdk_status_t mumble_sdk_server_boot_virtual_server(mumble_sdk_server_t *server, uint32_t server_id);
mumble_sdk_status_t mumble_sdk_server_kill_virtual_server(mumble_sdk_server_t *server, uint32_t server_id);
mumble_sdk_status_t mumble_sdk_server_set_superuser_password(mumble_sdk_server_t *server, uint32_t server_id,
															 const char *password);
mumble_sdk_status_t mumble_sdk_server_export_db_json(mumble_sdk_server_t *server, const char *output_path);
mumble_sdk_status_t mumble_sdk_server_import_db_json(mumble_sdk_server_t *server, const char *input_path);

#ifdef __cplusplus
}
#endif

#endif // MUMBLE_SDK_SERVER_H_
