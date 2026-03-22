// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Internal.h"

#include "Audio.h"
#include "Cert.h"
#include "Database.h"
#include "Global.h"
#include "Log.h"
#include "MainWindow.h"
#include "MumbleApplication.h"
#include "NetworkConfig.h"
#include "PluginManager.h"
#include "SDKHooks.h"
#include "SSL.h"
#include "ServerHandler.h"
#include "Settings.h"
#include "TalkingUI.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QProcessEnvironment>
#include <QUrl>

#include <chrono>
#include <new>
#include <optional>

extern void os_init();

#ifdef Q_OS_WIN
#	include <windows.h>
extern int os_early_init();
#endif

namespace Mumble {
namespace SDK {
namespace Internal {
	namespace {
		void emitClientEvent(ClientState *state, mumble_sdk_client_event_type_t type, const QString &message = QString(),
							 const QString &secondary = QString(), std::int64_t valueA = 0,
							 std::int64_t valueB = 0) {
			if (!state || !state->eventCallback) {
				return;
			}

			QByteArray messageUtf8   = message.toUtf8();
			QByteArray secondaryUtf8 = secondary.toUtf8();

			mumble_sdk_client_event_t eventData = {};
			eventData.size                      = sizeof(eventData);
			eventData.type                      = type;
			eventData.message                   = messageUtf8.constData();
			eventData.secondary_message         = secondaryUtf8.constData();
			eventData.value_a                   = valueA;
			eventData.value_b                   = valueB;

			state->eventCallback(state->eventUserdata, &eventData);
		}

		void installClientHooks(ClientState *state) {
			Hooks::setClientActive(state != nullptr);
			Hooks::setClientAutoAcceptInvalidCertificates(state && state->acceptInvalidCertificates);

			if (!state || !state->eventCallback) {
				Hooks::clearClientEventCallback();
				return;
			}

			Hooks::setClientEventCallback([state](Hooks::ClientEventType type, const QString &primary,
												  const QString &secondary, std::int64_t a, std::int64_t b) {
				switch (type) {
					case Hooks::ClientEventType::Connected:
						emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_CONNECTED, primary, secondary, a, b);
						break;
					case Hooks::ClientEventType::Synchronized:
						emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_SYNCHRONIZED, primary, secondary, a, b);
						break;
					case Hooks::ClientEventType::Disconnected:
						emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_DISCONNECTED, primary, secondary, a, b);
						break;
					case Hooks::ClientEventType::TextMessage:
						emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_TEXT_MESSAGE, primary, secondary, a, b);
						break;
					case Hooks::ClientEventType::Warning:
						emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_WARNING, primary, secondary, a, b);
						break;
					case Hooks::ClientEventType::ConnectionErr:
						emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_CONNECTION_ERROR, primary, secondary, a, b);
						break;
				}
			});
		}

		void installClientPositionalProvider(ClientState *state) {
			if (!state || !state->positionalProvider) {
				Hooks::clearClientPositionalDataProvider();
				return;
			}

			Hooks::setClientPositionalDataProvider([state](float *playerPos, float *playerDir, float *playerAxis,
														   float *cameraPos, float *cameraDir, float *cameraAxis,
														   QString &context, QString &identity) {
				mumble_sdk_positional_frame_t frame = {};
				frame.size                          = sizeof(frame);

				if (playerPos) {
					frame.player_pos = { playerPos[0], playerPos[1], playerPos[2] };
				}
				if (playerDir) {
					frame.player_dir = { playerDir[0], playerDir[1], playerDir[2] };
				}
				if (playerAxis) {
					frame.player_axis = { playerAxis[0], playerAxis[1], playerAxis[2] };
				}
				if (cameraPos) {
					frame.camera_pos = { cameraPos[0], cameraPos[1], cameraPos[2] };
				}
				if (cameraDir) {
					frame.camera_dir = { cameraDir[0], cameraDir[1], cameraDir[2] };
				}
				if (cameraAxis) {
					frame.camera_axis = { cameraAxis[0], cameraAxis[1], cameraAxis[2] };
				}

				QByteArray contextUtf8  = context.toUtf8();
				QByteArray identityUtf8 = identity.toUtf8();

				frame.context  = contextUtf8.constData();
				frame.identity = identityUtf8.constData();

				uint8_t ok = state->positionalProvider(state->positionalUserdata, &frame);
				if (!ok) {
					return false;
				}

				if (playerPos) {
					playerPos[0] = frame.player_pos.x;
					playerPos[1] = frame.player_pos.y;
					playerPos[2] = frame.player_pos.z;
				}
				if (playerDir) {
					playerDir[0] = frame.player_dir.x;
					playerDir[1] = frame.player_dir.y;
					playerDir[2] = frame.player_dir.z;
				}
				if (playerAxis) {
					playerAxis[0] = frame.player_axis.x;
					playerAxis[1] = frame.player_axis.y;
					playerAxis[2] = frame.player_axis.z;
				}
				if (cameraPos) {
					cameraPos[0] = frame.camera_pos.x;
					cameraPos[1] = frame.camera_pos.y;
					cameraPos[2] = frame.camera_pos.z;
				}
				if (cameraDir) {
					cameraDir[0] = frame.camera_dir.x;
					cameraDir[1] = frame.camera_dir.y;
					cameraDir[2] = frame.camera_dir.z;
				}
				if (cameraAxis) {
					cameraAxis[0] = frame.camera_axis.x;
					cameraAxis[1] = frame.camera_axis.y;
					cameraAxis[2] = frame.camera_axis.z;
				}

				context  = frame.context ? QString::fromUtf8(frame.context) : QString();
				identity = frame.identity ? QString::fromUtf8(frame.identity) : QString();

				return true;
			});
		}

		mumble_sdk_status_t ensureClientInitialized(ClientState *state) {
			if (!state) {
				return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
			}

			if (state->initialized) {
				return MUMBLE_SDK_STATUS_OK;
			}

#ifdef Q_OS_WIN
			if (os_early_init() != 0) {
				return MUMBLE_SDK_STATUS_INTERNAL_ERROR;
			}
			SetDllDirectoryW(L"");
#else
			os_init();
			qputenv("AVAHI_COMPAT_NOWARN", QByteArrayLiteral("1"));
#endif

			MumbleSSL::initialize();
			os_init();

			if (!state->configPath.empty()) {
				Global::g_global_struct = new Global(QString::fromUtf8(state->configPath.c_str()));
				Global::get().s.load(QString::fromUtf8(state->configPath.c_str()), true);
			} else {
				Global::g_global_struct = new Global();
				Global::get().s.load(true);
			}

			Global::get().s.audioWizardShown = true;
			Global::get().s.bReconnect       = false;
			Global::get().s.bPluginCheck     = false;
			Global::get().s.bUpdateCheck     = false;
			Global::get().s.bShowTalkingUI   = false;

			DeferInit::run_initializers();
			NetworkConfig::SetupProxy();

			Global::get().nam = new QNetworkAccessManager();
			Global::get().db  = new Database(QLatin1String("sdk-client"));
			Global::get().db->clearLocalMuted();

			Global::get().pluginManager = new PluginManager();
			installClientHooks(state);
			installClientPositionalProvider(state);

			Global::get().mw = new MainWindow(nullptr);
			Global::get().mw->hide();

			Global::get().talkingUI = new TalkingUI();
			Global::get().talkingUI->setVisible(false);

			QObject::connect(Global::get().mw, &MainWindow::userAddedChannelListener, Global::get().talkingUI,
							 &TalkingUI::on_channelListenerAdded);
			QObject::connect(Global::get().mw, &MainWindow::userRemovedChannelListener, Global::get().talkingUI,
							 &TalkingUI::on_channelListenerRemoved);
			QObject::connect(Global::get().channelListenerManager.get(),
							 &ChannelListenerManager::localVolumeAdjustmentsChanged, Global::get().talkingUI,
							 &TalkingUI::on_channelListenerLocalVolumeAdjustmentChanged);
			QObject::connect(Global::get().mw, &MainWindow::serverSynchronized, Global::get().talkingUI,
							 &TalkingUI::on_serverSynchronized);

			Global::get().l = new Log();
			Global::get().l->processDeferredLogs();

			if (!state->certificatePkcs12Path.empty()) {
				QFile certFile(QString::fromUtf8(state->certificatePkcs12Path.c_str()));
				if (!certFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
					return MUMBLE_SDK_STATUS_NOT_FOUND;
				}
				Global::get().s.kpCertificate =
					CertWizard::importCert(certFile.readAll(), QString::fromUtf8(state->certificatePkcs12Password.c_str()));
			}

			if (!CertWizard::validateCert(Global::get().s.kpCertificate)) {
				Global::get().s.kpCertificate = CertWizard::generateNewCert();

				if (!state->defaultCertificateDir.empty()) {
					QDir certDir(QString::fromUtf8(state->defaultCertificateDir.c_str()));
					certDir.mkpath(QStringLiteral("."));
					QFile backup(certDir.absoluteFilePath(QStringLiteral("MumbleAutomaticCertificateBackup.p12")));
					if (backup.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered)) {
						backup.write(CertWizard::exportCert(Global::get().s.kpCertificate));
					}
				}
			}

			Audio::start(QString::fromUtf8(state->inputDevice.c_str()), QString::fromUtf8(state->outputDevice.c_str()));

			QObject::connect(Global::get().mw, &MainWindow::serverSynchronized, Global::get().mw, [state]() {
				state->connected = true;
				emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_SYNCHRONIZED);
			});

			state->initialized = true;
			return MUMBLE_SDK_STATUS_OK;
		}

		void shutdownClient(ClientState *state) {
			if (!state || !state->initialized) {
				return;
			}

			ServerHandlerPtr sh = Global::get().sh;
			if (sh) {
				if (sh->isRunning()) {
					Global::get().mw->on_qaServerDisconnect_triggered();
					sh->disconnect();
					int iterations = 0;
					while (!sh->wait(10) && iterations < 200) {
						QCoreApplication::processEvents();
						iterations++;
					}
				}
			}

			QCoreApplication::processEvents();
			Audio::stop();

			delete Global::get().talkingUI;
			Global::get().talkingUI = nullptr;

			delete Global::get().mw;
			Global::get().mw = nullptr;

			Global::get().sh.reset();

			delete Global::get().nam;
			Global::get().nam = nullptr;

			delete Global::get().db;
			Global::get().db = nullptr;

			delete Global::get().l;
			Global::get().l = nullptr;

			delete Global::get().pluginManager;
			Global::get().pluginManager = nullptr;

			DeferInit::run_destroyers();

			delete Global::g_global_struct;
			Global::g_global_struct = nullptr;

			Hooks::clearClientPositionalDataProvider();
			Hooks::clearClientEventCallback();
			Hooks::setClientAutoAcceptInvalidCertificates(false);
			Hooks::setClientActive(false);

			state->connected   = false;
			state->initialized = false;
		}
	} // namespace
} // namespace Internal
} // namespace SDK
} // namespace Mumble

extern "C" {

mumble_sdk_status_t mumble_sdk_client_create(mumble_sdk_runtime_t *runtime, mumble_sdk_client_t **out_client) {
	if (!runtime || !runtime->state || !out_client) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	if (runtime->state->client) {
		return MUMBLE_SDK_STATUS_ALREADY_EXISTS;
	}

	auto clientState      = std::make_unique< Mumble::SDK::Internal::ClientState >();
	clientState->runtime  = runtime->state;
	runtime->state->clientCount++;

	mumble_sdk_client_t *client = new (std::nothrow) mumble_sdk_client_t();
	if (!client) {
		runtime->state->clientCount--;
		return MUMBLE_SDK_STATUS_INTERNAL_ERROR;
	}

	client->state          = clientState.get();
	runtime->state->client = std::move(clientState);
	*out_client            = client;
	return MUMBLE_SDK_STATUS_OK;
}

void mumble_sdk_client_destroy(mumble_sdk_client_t *client) {
	if (!client || !client->state) {
		return;
	}

	Mumble::SDK::Internal::RuntimeState *runtime = client->state->runtime;

	Mumble::SDK::Internal::invoke(runtime, [state = client->state]() {
		Mumble::SDK::Internal::shutdownClient(state);
	});

	runtime->client.reset();
	runtime->clientCount--;
	Mumble::SDK::Internal::maybeStopRuntime(runtime);
	delete client;
}

mumble_sdk_status_t mumble_sdk_client_set_event_callback(mumble_sdk_client_t *client,
														 mumble_sdk_client_event_callback_t callback, void *userdata) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->eventCallback = callback;
	client->state->eventUserdata = userdata;

	return Mumble::SDK::Internal::invoke(client->state->runtime, [state = client->state]() {
		Mumble::SDK::Internal::installClientHooks(state);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_config_path(mumble_sdk_client_t *client, const char *config_path) {
	if (!client || !client->state || client->state->initialized) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->configPath = config_path ? config_path : "";
	return MUMBLE_SDK_STATUS_OK;
}

mumble_sdk_status_t mumble_sdk_client_set_default_certificate_dir(mumble_sdk_client_t *client, const char *directory) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->defaultCertificateDir = directory ? directory : "";
	return MUMBLE_SDK_STATUS_OK;
}

mumble_sdk_status_t mumble_sdk_client_set_certificate_pkcs12_file(mumble_sdk_client_t *client, const char *path,
																  const char *password) {
	if (!client || !client->state || !path) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->certificatePkcs12Path     = path;
	client->state->certificatePkcs12Password = password ? password : "";
	return MUMBLE_SDK_STATUS_OK;
}

mumble_sdk_status_t mumble_sdk_client_set_audio_input_device(mumble_sdk_client_t *client, const char *device_name) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->inputDevice = device_name ? device_name : "";

	if (!client->state->initialized) {
		return MUMBLE_SDK_STATUS_OK;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [state = client->state]() {
		Audio::stop();
		Audio::start(QString::fromUtf8(state->inputDevice.c_str()), QString::fromUtf8(state->outputDevice.c_str()));
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_audio_output_device(mumble_sdk_client_t *client, const char *device_name) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->outputDevice = device_name ? device_name : "";

	if (!client->state->initialized) {
		return MUMBLE_SDK_STATUS_OK;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [state = client->state]() {
		Audio::stop();
		Audio::start(QString::fromUtf8(state->inputDevice.c_str()), QString::fromUtf8(state->outputDevice.c_str()));
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_accept_invalid_certificates(mumble_sdk_client_t *client, uint8_t enabled) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->acceptInvalidCertificates = enabled != 0;
	return Mumble::SDK::Internal::invoke(client->state->runtime, [state = client->state]() {
		Mumble::SDK::Hooks::setClientAutoAcceptInvalidCertificates(state->acceptInvalidCertificates);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_positional_provider(mumble_sdk_client_t *client,
															  mumble_sdk_positional_provider_t provider,
															  void *userdata) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	client->state->positionalProvider = provider;
	client->state->positionalUserdata = userdata;

	return Mumble::SDK::Internal::invoke(client->state->runtime, [state = client->state]() {
		Mumble::SDK::Internal::installClientPositionalProvider(state);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_connect(mumble_sdk_client_t *client, const char *host, uint16_t port,
											  const char *username, const char *password, const char *channel_path) {
	if (!client || !client->state || !host || !username) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [state = client->state, hostString = std::string(host),
																	 port, usernameString = std::string(username),
																	 passwordString = std::string(password ? password : ""),
																	 channelPathString = std::string(channel_path ? channel_path : "")]() {
		mumble_sdk_status_t status = Mumble::SDK::Internal::ensureClientInitialized(state);
		if (status != MUMBLE_SDK_STATUS_OK) {
			return status;
		}

		Global::get().s.qsUsername = QString::fromUtf8(usernameString.c_str());

		QUrl url;
		url.setScheme(QStringLiteral("mumble"));
		url.setHost(QString::fromUtf8(hostString.c_str()));
		url.setPort(port);
		url.setUserName(QString::fromUtf8(usernameString.c_str()));
		if (!passwordString.empty()) {
			url.setPassword(QString::fromUtf8(passwordString.c_str()));
		}
		if (!channelPathString.empty()) {
			QString path = QString::fromUtf8(channelPathString.c_str());
			if (!path.startsWith(QLatin1Char('/'))) {
				path.prepend(QLatin1Char('/'));
			}
			url.setPath(path);
		}

		Global::get().mw->openUrl(url);

		ServerHandlerPtr sh = Global::get().sh;
		if (sh) {
			QObject::connect(sh.get(), &ServerHandler::connected, Global::get().mw, [state]() {
				Mumble::SDK::Internal::emitClientEvent(state, MUMBLE_SDK_CLIENT_EVENT_CONNECTED);
			});
		}

		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_disconnect(mumble_sdk_client_t *client) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, []() {
		ServerHandlerPtr sh = Global::get().sh;
		if (sh && sh->isRunning()) {
			Global::get().mw->on_qaServerDisconnect_triggered();
			sh->disconnect();
		}
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_mute(mumble_sdk_client_t *client, uint8_t mute) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [mute]() {
		if (!Global::get().mw) {
			return MUMBLE_SDK_STATUS_BUSY;
		}
		Global::get().mw->setAudioMute(mute != 0);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_deaf(mumble_sdk_client_t *client, uint8_t deaf) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [deaf]() {
		if (!Global::get().mw) {
			return MUMBLE_SDK_STATUS_BUSY;
		}
		Global::get().mw->setAudioDeaf(deaf != 0);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_set_push_to_talk(mumble_sdk_client_t *client, uint8_t pressed) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [pressed]() {
		if (!Global::get().mw) {
			return MUMBLE_SDK_STATUS_BUSY;
		}
		Global::get().mw->on_PushToTalk_triggered(pressed != 0, QVariant());
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_join_channel(mumble_sdk_client_t *client, uint32_t channel_id) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [channel_id]() {
		if (!Global::get().sh || Global::get().uiSession == 0) {
			return MUMBLE_SDK_STATUS_BUSY;
		}
		Global::get().sh->joinChannel(Global::get().uiSession, channel_id);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_listen_channel(mumble_sdk_client_t *client, uint32_t channel_id,
													 uint8_t enabled) {
	if (!client || !client->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime, [channel_id, enabled]() {
		if (!Global::get().sh) {
			return MUMBLE_SDK_STATUS_BUSY;
		}
		if (enabled) {
			Global::get().sh->startListeningToChannel(channel_id);
		} else {
			Global::get().sh->stopListeningToChannel(channel_id);
		}
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_client_send_channel_text(mumble_sdk_client_t *client, uint32_t channel_id,
														const char *message, uint8_t tree) {
	if (!client || !client->state || !message) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime,
										 [channel_id, messageString = std::string(message), tree]() {
											 if (!Global::get().sh) {
												 return MUMBLE_SDK_STATUS_BUSY;
											 }
											 Global::get().sh->sendChannelTextMessage(
												 channel_id, QString::fromUtf8(messageString.c_str()), tree != 0);
											 return MUMBLE_SDK_STATUS_OK;
										 });
}

mumble_sdk_status_t mumble_sdk_client_send_user_text(mumble_sdk_client_t *client, uint32_t session_id,
													 const char *message) {
	if (!client || !client->state || !message) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(client->state->runtime,
										 [session_id, messageString = std::string(message)]() {
											 if (!Global::get().sh) {
												 return MUMBLE_SDK_STATUS_BUSY;
											 }
											 Global::get().sh->sendUserTextMessage(session_id,
																					QString::fromUtf8(messageString.c_str()));
											 return MUMBLE_SDK_STATUS_OK;
										 });
}

} // extern "C"
