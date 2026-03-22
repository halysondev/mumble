#include <mumble-sdk/runtime.h>
#include <mumble-sdk/server.h>

#include <cstdio>

int main() {
	mumble_sdk_runtime_t *runtime = nullptr;
	if (mumble_sdk_runtime_create(&runtime) != MUMBLE_SDK_STATUS_OK || !runtime) {
		std::fprintf(stderr, "Failed to create runtime\n");
		return 1;
	}

	mumble_sdk_server_t *server = nullptr;
	if (mumble_sdk_server_create(runtime, &server) != MUMBLE_SDK_STATUS_OK || !server) {
		std::fprintf(stderr, "Failed to create server\n");
		mumble_sdk_runtime_destroy(runtime);
		return 2;
	}

	mumble_sdk_server_destroy(server);
	mumble_sdk_runtime_destroy(runtime);
	return 0;
}
