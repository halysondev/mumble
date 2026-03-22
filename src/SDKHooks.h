// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SDKHOOKS_H_
#define MUMBLE_SDKHOOKS_H_

#include <QString>

#include <cstdint>
#include <functional>

namespace Mumble {
namespace SDK {
namespace Hooks {

	enum class ClientEventType : std::int32_t {
		Connected     = 1,
		Synchronized  = 2,
		Disconnected  = 3,
		TextMessage   = 4,
		Warning       = 5,
		ConnectionErr = 6
	};

	enum class ServerEventType : std::int32_t {
		Started              = 1,
		Stopped              = 2,
		VirtualServerStarted = 3,
		VirtualServerStopped = 4
	};

	using RuntimeLogCallback = std::function< void(std::int32_t level, const QString &message) >;
	using ClientEventCallback =
		std::function< void(ClientEventType type, const QString &primary, const QString &secondary, std::int64_t a,
							std::int64_t b) >;
	using ServerEventCallback =
		std::function< void(ServerEventType type, const QString &primary, const QString &secondary, std::int64_t a,
							std::int64_t b) >;
	using ClientPositionalDataProvider =
		std::function< bool(float *playerPos, float *playerDir, float *playerAxis, float *cameraPos, float *cameraDir,
							float *cameraAxis, QString &context, QString &identity) >;

	void setRuntimeLogCallback(RuntimeLogCallback callback);
	void clearRuntimeLogCallback();
	void emitRuntimeLog(std::int32_t level, const QString &message);

	void setClientEventCallback(ClientEventCallback callback);
	void clearClientEventCallback();
	void emitClientEvent(ClientEventType type, const QString &primary = QString(), const QString &secondary = QString(),
						 std::int64_t a = 0, std::int64_t b = 0);

	void setServerEventCallback(ServerEventCallback callback);
	void clearServerEventCallback();
	void emitServerEvent(ServerEventType type, const QString &primary = QString(), const QString &secondary = QString(),
						 std::int64_t a = 0, std::int64_t b = 0);

	void setClientActive(bool active);
	bool isClientActive();

	void setClientAutoAcceptInvalidCertificates(bool enabled);
	bool clientAutoAcceptInvalidCertificates();

	void setClientPositionalDataProvider(ClientPositionalDataProvider provider);
	void clearClientPositionalDataProvider();
	bool hasClientPositionalDataProvider();
	bool fetchClientPositionalData(float *playerPos, float *playerDir, float *playerAxis, float *cameraPos,
								   float *cameraDir, float *cameraAxis, QString &context, QString &identity);

} // namespace Hooks
} // namespace SDK
} // namespace Mumble

#endif // MUMBLE_SDKHOOKS_H_
