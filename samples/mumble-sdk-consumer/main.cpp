#include <mumble-sdk/client.h>
#include <mumble-sdk/runtime.h>

#include <cstdio>

int main() {
	mumble_sdk_runtime_t *runtime = nullptr;
	if (mumble_sdk_runtime_create(&runtime) != MUMBLE_SDK_STATUS_OK || !runtime) {
		std::fprintf(stderr, "Failed to create runtime\n");
		return 1;
	}

	mumble_sdk_client_t *client = nullptr;
	if (mumble_sdk_client_create(runtime, &client) != MUMBLE_SDK_STATUS_OK || !client) {
		std::fprintf(stderr, "Failed to create client\n");
		mumble_sdk_runtime_destroy(runtime);
		return 2;
	}

	mumble_sdk_client_destroy(client);
	mumble_sdk_runtime_destroy(runtime);

	return 0;
}
