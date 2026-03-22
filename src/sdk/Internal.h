// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SDK_INTERNAL_H_
#define MUMBLE_SDK_INTERNAL_H_

#include "mumble-sdk/client.h"
#include "mumble-sdk/runtime.h"
#include "mumble-sdk/server.h"

#include <QApplication>

#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

struct mumble_sdk_runtime;
struct mumble_sdk_client;
struct mumble_sdk_server;

namespace Mumble {
namespace SDK {
namespace Internal {

	struct ClientState;
	struct ServerState;

	struct RuntimeState {
		std::mutex mutex;
		std::condition_variable cv;
		std::deque< std::function< void() > > tasks;
		std::thread worker;
		std::thread::id workerId;
		QApplication *application = nullptr;
		bool ready                = false;
		bool stopping             = false;
		std::string startupError;

		std::size_t runtimeRefCount = 0;
		std::size_t clientCount     = 0;
		std::size_t serverCount     = 0;

		mumble_sdk_log_callback_t logCallback = nullptr;
		void *logUserdata                     = nullptr;

		std::unique_ptr< ClientState > client;
		std::unique_ptr< ServerState > server;
	};

	struct ClientState {
		RuntimeState *runtime = nullptr;

		mumble_sdk_client_event_callback_t eventCallback = nullptr;
		void *eventUserdata                              = nullptr;
		mumble_sdk_positional_provider_t positionalProvider = nullptr;
		void *positionalUserdata                            = nullptr;

		std::string configPath;
		std::string defaultCertificateDir;
		std::string certificatePkcs12Path;
		std::string certificatePkcs12Password;
		std::string inputDevice;
		std::string outputDevice;

		bool initialized                = false;
		bool connected                  = false;
		bool acceptInvalidCertificates  = false;
	};

	struct ServerState {
		RuntimeState *runtime = nullptr;

		mumble_sdk_server_event_callback_t eventCallback = nullptr;
		void *eventUserdata                              = nullptr;

		std::string iniFilePath;
		std::map< std::string, std::string > configOverrides;

		bool started = false;
	};

	RuntimeState *acquireRuntimeState();
	void releaseRuntimeState(RuntimeState *state);
	void maybeStopRuntime(RuntimeState *state);

	mumble_sdk_status_t ensureRuntimeStarted(RuntimeState *state);

	template< typename Fn > auto invoke(RuntimeState *state, Fn &&fn) -> decltype(fn()) {
		using Result = decltype(fn());

		if (std::this_thread::get_id() == state->workerId) {
			if constexpr (std::is_void_v< Result >) {
				fn();
				return;
			} else {
				return fn();
			}
		}

		auto promise = std::make_shared< std::promise< Result > >();
		auto future  = promise->get_future();

		{
			std::lock_guard< std::mutex > lock(state->mutex);
			state->tasks.emplace_back([promise, fn = std::forward< Fn >(fn)]() mutable {
				try {
					if constexpr (std::is_void_v< Result >) {
						fn();
						promise->set_value();
					} else {
						promise->set_value(fn());
					}
				} catch (...) {
					promise->set_exception(std::current_exception());
				}
			});
		}

		state->cv.notify_one();

		if constexpr (std::is_void_v< Result >) {
			future.get();
			return;
		} else {
			return future.get();
		}
	}

	char *duplicateUtf8(const QString &text);
	char *duplicateUtf8(const std::string &text);

} // namespace Internal
} // namespace SDK
} // namespace Mumble

struct mumble_sdk_runtime {
	Mumble::SDK::Internal::RuntimeState *state = nullptr;
};

struct mumble_sdk_client {
	Mumble::SDK::Internal::ClientState *state = nullptr;
};

struct mumble_sdk_server {
	Mumble::SDK::Internal::ServerState *state = nullptr;
};

#endif // MUMBLE_SDK_INTERNAL_H_
