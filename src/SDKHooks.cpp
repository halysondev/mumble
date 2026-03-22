// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "SDKHooks.h"

#include <QMutex>
#include <QMutexLocker>

namespace Mumble {
namespace SDK {
namespace Hooks {
	namespace {
		struct HookState {
			QMutex lock;
			RuntimeLogCallback runtimeLogCallback;
			ClientEventCallback clientEventCallback;
			ServerEventCallback serverEventCallback;
			ClientPositionalDataProvider positionalDataProvider;
			bool clientActive                    = false;
			bool autoAcceptInvalidCertificates   = false;
		};

		HookState &state() {
			static HookState hookState;
			return hookState;
		}
	} // namespace

	void setRuntimeLogCallback(RuntimeLogCallback callback) {
		QMutexLocker locker(&state().lock);
		state().runtimeLogCallback = std::move(callback);
	}

	void clearRuntimeLogCallback() {
		QMutexLocker locker(&state().lock);
		state().runtimeLogCallback = RuntimeLogCallback();
	}

	void emitRuntimeLog(std::int32_t level, const QString &message) {
		RuntimeLogCallback callback;
		{
			QMutexLocker locker(&state().lock);
			callback = state().runtimeLogCallback;
		}

		if (callback) {
			callback(level, message);
		}
	}

	void setClientEventCallback(ClientEventCallback callback) {
		QMutexLocker locker(&state().lock);
		state().clientEventCallback = std::move(callback);
	}

	void clearClientEventCallback() {
		QMutexLocker locker(&state().lock);
		state().clientEventCallback = ClientEventCallback();
	}

	void emitClientEvent(ClientEventType type, const QString &primary, const QString &secondary, std::int64_t a,
						 std::int64_t b) {
		ClientEventCallback callback;
		{
			QMutexLocker locker(&state().lock);
			callback = state().clientEventCallback;
		}

		if (callback) {
			callback(type, primary, secondary, a, b);
		}
	}

	void setServerEventCallback(ServerEventCallback callback) {
		QMutexLocker locker(&state().lock);
		state().serverEventCallback = std::move(callback);
	}

	void clearServerEventCallback() {
		QMutexLocker locker(&state().lock);
		state().serverEventCallback = ServerEventCallback();
	}

	void emitServerEvent(ServerEventType type, const QString &primary, const QString &secondary, std::int64_t a,
						 std::int64_t b) {
		ServerEventCallback callback;
		{
			QMutexLocker locker(&state().lock);
			callback = state().serverEventCallback;
		}

		if (callback) {
			callback(type, primary, secondary, a, b);
		}
	}

	void setClientActive(bool active) {
		QMutexLocker locker(&state().lock);
		state().clientActive = active;
	}

	bool isClientActive() {
		QMutexLocker locker(&state().lock);
		return state().clientActive;
	}

	void setClientAutoAcceptInvalidCertificates(bool enabled) {
		QMutexLocker locker(&state().lock);
		state().autoAcceptInvalidCertificates = enabled;
	}

	bool clientAutoAcceptInvalidCertificates() {
		QMutexLocker locker(&state().lock);
		return state().autoAcceptInvalidCertificates;
	}

	void setClientPositionalDataProvider(ClientPositionalDataProvider provider) {
		QMutexLocker locker(&state().lock);
		state().positionalDataProvider = std::move(provider);
	}

	void clearClientPositionalDataProvider() {
		QMutexLocker locker(&state().lock);
		state().positionalDataProvider = ClientPositionalDataProvider();
	}

	bool hasClientPositionalDataProvider() {
		QMutexLocker locker(&state().lock);
		return static_cast< bool >(state().positionalDataProvider);
	}

	bool fetchClientPositionalData(float *playerPos, float *playerDir, float *playerAxis, float *cameraPos,
								   float *cameraDir, float *cameraAxis, QString &context, QString &identity) {
		ClientPositionalDataProvider provider;
		{
			QMutexLocker locker(&state().lock);
			provider = state().positionalDataProvider;
		}

		if (!provider) {
			return false;
		}

		return provider(playerPos, playerDir, playerAxis, cameraPos, cameraDir, cameraAxis, context, identity);
	}

} // namespace Hooks
} // namespace SDK
} // namespace Mumble
