// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SDK_RUNTIME_H_
#define MUMBLE_SDK_RUNTIME_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

mumble_sdk_status_t mumble_sdk_runtime_create(mumble_sdk_runtime_t **out_runtime);
void mumble_sdk_runtime_destroy(mumble_sdk_runtime_t *runtime);

mumble_sdk_status_t mumble_sdk_runtime_set_log_callback(mumble_sdk_runtime_t *runtime,
														mumble_sdk_log_callback_t callback, void *userdata);

void mumble_sdk_free(void *memory);

#ifdef __cplusplus
}
#endif

#endif // MUMBLE_SDK_RUNTIME_H_
