// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SDK_CLIENT_H_
#define MUMBLE_SDK_CLIENT_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

mumble_sdk_status_t mumble_sdk_client_create(mumble_sdk_runtime_t *runtime, mumble_sdk_client_t **out_client);
void mumble_sdk_client_destroy(mumble_sdk_client_t *client);

mumble_sdk_status_t mumble_sdk_client_set_event_callback(mumble_sdk_client_t *client,
														 mumble_sdk_client_event_callback_t callback, void *userdata);
mumble_sdk_status_t mumble_sdk_client_set_config_path(mumble_sdk_client_t *client, const char *config_path);
mumble_sdk_status_t mumble_sdk_client_set_default_certificate_dir(mumble_sdk_client_t *client, const char *directory);
mumble_sdk_status_t mumble_sdk_client_set_certificate_pkcs12_file(mumble_sdk_client_t *client, const char *path,
																  const char *password);
mumble_sdk_status_t mumble_sdk_client_set_audio_input_device(mumble_sdk_client_t *client, const char *device_name);
mumble_sdk_status_t mumble_sdk_client_set_audio_output_device(mumble_sdk_client_t *client, const char *device_name);
mumble_sdk_status_t mumble_sdk_client_set_accept_invalid_certificates(mumble_sdk_client_t *client, uint8_t enabled);
mumble_sdk_status_t mumble_sdk_client_set_positional_provider(mumble_sdk_client_t *client,
															  mumble_sdk_positional_provider_t provider,
															  void *userdata);

mumble_sdk_status_t mumble_sdk_client_connect(mumble_sdk_client_t *client, const char *host, uint16_t port,
											  const char *username, const char *password, const char *channel_path);
mumble_sdk_status_t mumble_sdk_client_disconnect(mumble_sdk_client_t *client);

mumble_sdk_status_t mumble_sdk_client_set_mute(mumble_sdk_client_t *client, uint8_t mute);
mumble_sdk_status_t mumble_sdk_client_set_deaf(mumble_sdk_client_t *client, uint8_t deaf);
mumble_sdk_status_t mumble_sdk_client_set_push_to_talk(mumble_sdk_client_t *client, uint8_t pressed);

mumble_sdk_status_t mumble_sdk_client_join_channel(mumble_sdk_client_t *client, uint32_t channel_id);
mumble_sdk_status_t mumble_sdk_client_listen_channel(mumble_sdk_client_t *client, uint32_t channel_id,
													 uint8_t enabled);
mumble_sdk_status_t mumble_sdk_client_send_channel_text(mumble_sdk_client_t *client, uint32_t channel_id,
														const char *message, uint8_t tree);
mumble_sdk_status_t mumble_sdk_client_send_user_text(mumble_sdk_client_t *client, uint32_t session_id,
													 const char *message);

#ifdef __cplusplus
}
#endif

#endif // MUMBLE_SDK_CLIENT_H_
