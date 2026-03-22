// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Internal.h"

#include "Logger.h"
#include "SDKHooks.h"

#include <QApplication>
#include <QByteArray>
#include <QEventLoop>
#include <QString>

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <exception>
#include <memory>

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

namespace Mumble {
namespace SDK {
namespace Internal {
	namespace {
		class SDKApplication : public QApplication {
		public:
			using QApplication::QApplication;

			bool notify(QObject *receiver, QEvent *event) override {
				try {
					return QApplication::notify(receiver, event);
				} catch (const std::exception &e) {
					Hooks::emitRuntimeLog(static_cast< std::int32_t >(MUMBLE_SDK_LOG_FATAL),
										 QString::fromLatin1("Unhandled exception: %1").arg(QString::fromUtf8(e.what())));
					std::abort();
				} catch (...) {
					Hooks::emitRuntimeLog(static_cast< std::int32_t >(MUMBLE_SDK_LOG_FATAL),
										 QStringLiteral("Unhandled non-standard exception"));
					std::abort();
				}
			}
		};

		class SDKLogSink final : public spdlog::sinks::base_sink< spdlog::details::null_mutex > {
		protected:
			void sink_it_(const spdlog::details::log_msg &msg) override {
				spdlog::memory_buf_t formatted;
				base_sink< spdlog::details::null_mutex >::formatter_->format(msg, formatted);
				QString text = QString::fromUtf8(formatted.data(), static_cast< int >(formatted.size() ));
				Hooks::emitRuntimeLog(static_cast< std::int32_t >(msg.level), text);
			}

			void flush_() override {
			}
		};

		std::mutex g_runtimeMutex;
		std::unique_ptr< RuntimeState > g_runtimeState;

		void runtimeThreadMain(RuntimeState *state) {
			int argc     = 1;
			char arg0[]  = "mumble-sdk";
			char *argv[] = { arg0, nullptr };

			try {
				SDKApplication application(argc, argv);
				application.setApplicationName(QStringLiteral("MumbleSDK"));
				application.setOrganizationName(QStringLiteral("Mumble"));
				application.setOrganizationDomain(QStringLiteral("mumble.info"));
				application.setQuitOnLastWindowClosed(false);

				state->application = &application;
				state->workerId    = std::this_thread::get_id();

				mumble::log::init(spdlog::level::trace);
				mumble::log::addSink(std::make_shared< SDKLogSink >());

				{
					std::lock_guard< std::mutex > lock(state->mutex);
					state->ready = true;
				}
				state->cv.notify_all();

				while (true) {
					std::function< void() > task;

					{
						std::unique_lock< std::mutex > lock(state->mutex);
						if (state->tasks.empty() && !state->stopping) {
							state->cv.wait_for(lock, std::chrono::milliseconds(5));
						}

						if (!state->tasks.empty()) {
							task = std::move(state->tasks.front());
							state->tasks.pop_front();
						} else if (state->stopping) {
							break;
						}
					}

					if (task) {
						task();
					}

					application.processEvents(QEventLoop::AllEvents, 10);
				}

				application.processEvents(QEventLoop::AllEvents, 50);
				state->application = nullptr;
			} catch (const std::exception &e) {
				{
					std::lock_guard< std::mutex > lock(state->mutex);
					state->startupError = e.what();
					state->ready        = true;
				}
				state->cv.notify_all();
			} catch (...) {
				{
					std::lock_guard< std::mutex > lock(state->mutex);
					state->startupError = "unknown startup failure";
					state->ready        = true;
				}
				state->cv.notify_all();
			}
		}
	} // namespace

	RuntimeState *acquireRuntimeState() {
		std::lock_guard< std::mutex > lock(g_runtimeMutex);
		if (!g_runtimeState) {
			g_runtimeState = std::make_unique< RuntimeState >();
		}

		g_runtimeState->runtimeRefCount++;
		return g_runtimeState.get();
	}

	void releaseRuntimeState(RuntimeState *state) {
		if (!state) {
			return;
		}

		{
			std::lock_guard< std::mutex > lock(g_runtimeMutex);
			if (g_runtimeState.get() == state && g_runtimeState->runtimeRefCount > 0) {
				g_runtimeState->runtimeRefCount--;
			}
		}

		maybeStopRuntime(state);
	}

	void maybeStopRuntime(RuntimeState *state) {
		if (!state) {
			return;
		}

		bool shouldStop = false;
		{
			std::lock_guard< std::mutex > runtimeLock(g_runtimeMutex);
			if (g_runtimeState.get() != state) {
				return;
			}

			shouldStop = g_runtimeState->runtimeRefCount == 0 && g_runtimeState->clientCount == 0
						 && g_runtimeState->serverCount == 0;
			if (shouldStop) {
				{
					std::lock_guard< std::mutex > lock(g_runtimeState->mutex);
					g_runtimeState->stopping = true;
				}
				g_runtimeState->cv.notify_all();
			}
		}

		if (!shouldStop) {
			return;
		}

		if (state->worker.joinable()) {
			state->worker.join();
		}

		std::lock_guard< std::mutex > lock(g_runtimeMutex);
		if (g_runtimeState.get() == state) {
			Hooks::clearRuntimeLogCallback();
			g_runtimeState.reset();
		}
	}

	mumble_sdk_status_t ensureRuntimeStarted(RuntimeState *state) {
		if (!state) {
			return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
		}

		{
			std::lock_guard< std::mutex > lock(state->mutex);
			if (state->ready && state->startupError.empty()) {
				return MUMBLE_SDK_STATUS_OK;
			}
			if (!state->worker.joinable()) {
				state->worker = std::thread(runtimeThreadMain, state);
			}
		}

		std::unique_lock< std::mutex > lock(state->mutex);
		state->cv.wait(lock, [state]() { return state->ready; });

		if (!state->startupError.empty()) {
			return MUMBLE_SDK_STATUS_INTERNAL_ERROR;
		}

		return MUMBLE_SDK_STATUS_OK;
	}

	char *duplicateUtf8(const QString &text) {
		QByteArray utf8 = text.toUtf8();
		char *buffer    = static_cast< char * >(std::malloc(static_cast< std::size_t >(utf8.size()) + 1));
		if (!buffer) {
			return nullptr;
		}

		std::memcpy(buffer, utf8.constData(), static_cast< std::size_t >(utf8.size()));
		buffer[utf8.size()] = '\0';
		return buffer;
	}

	char *duplicateUtf8(const std::string &text) {
		char *buffer = static_cast< char * >(std::malloc(text.size() + 1));
		if (!buffer) {
			return nullptr;
		}

		std::memcpy(buffer, text.data(), text.size());
		buffer[text.size()] = '\0';
		return buffer;
	}

} // namespace Internal
} // namespace SDK
} // namespace Mumble

extern "C" {

mumble_sdk_status_t mumble_sdk_runtime_create(mumble_sdk_runtime_t **out_runtime) {
	if (!out_runtime) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	mumble_sdk_runtime_t *runtime = new (std::nothrow) mumble_sdk_runtime_t();
	if (!runtime) {
		return MUMBLE_SDK_STATUS_INTERNAL_ERROR;
	}

	runtime->state = Mumble::SDK::Internal::acquireRuntimeState();
	mumble_sdk_status_t status = Mumble::SDK::Internal::ensureRuntimeStarted(runtime->state);
	if (status != MUMBLE_SDK_STATUS_OK) {
		Mumble::SDK::Internal::releaseRuntimeState(runtime->state);
		delete runtime;
		return status;
	}

	*out_runtime = runtime;
	return MUMBLE_SDK_STATUS_OK;
}

void mumble_sdk_runtime_destroy(mumble_sdk_runtime_t *runtime) {
	if (!runtime) {
		return;
	}

	Mumble::SDK::Internal::releaseRuntimeState(runtime->state);
	delete runtime;
}

mumble_sdk_status_t mumble_sdk_runtime_set_log_callback(mumble_sdk_runtime_t *runtime,
														mumble_sdk_log_callback_t callback, void *userdata) {
	if (!runtime || !runtime->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	runtime->state->logCallback = callback;
	runtime->state->logUserdata = userdata;

	if (!callback) {
		Mumble::SDK::Hooks::clearRuntimeLogCallback();
		return MUMBLE_SDK_STATUS_OK;
	}

	Mumble::SDK::Internal::RuntimeState *state = runtime->state;
	Mumble::SDK::Hooks::setRuntimeLogCallback([state](std::int32_t level, const QString &message) {
		if (!state->logCallback) {
			return;
		}

		QByteArray utf8 = message.toUtf8();
		state->logCallback(state->logUserdata, static_cast< mumble_sdk_log_level_t >(level), utf8.constData());
	});

	return MUMBLE_SDK_STATUS_OK;
}

void mumble_sdk_free(void *memory) {
	std::free(memory);
}

} // extern "C"
