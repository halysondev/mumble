// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Internal.h"

#include "Meta.h"
#include "Server.h"
#include "SSL.h"

#include <QFile>
#include <QSettings>
#include <QString>

#include <fstream>
#include <new>

#include <nlohmann/json.hpp>

extern QFile *qfLog;
extern Meta *meta;

namespace Mumble {
namespace SDK {
namespace Internal {
	namespace {
		void emitServerEvent(ServerState *state, mumble_sdk_server_event_type_t type, const QString &message = QString(),
							 const QString &secondary = QString(), std::int64_t valueA = 0,
							 std::int64_t valueB = 0) {
			if (!state || !state->eventCallback) {
				return;
			}

			QByteArray messageUtf8   = message.toUtf8();
			QByteArray secondaryUtf8 = secondary.toUtf8();

			mumble_sdk_server_event_t eventData = {};
			eventData.size                      = sizeof(eventData);
			eventData.type                      = type;
			eventData.message                   = messageUtf8.constData();
			eventData.secondary_message         = secondaryUtf8.constData();
			eventData.value_a                   = valueA;
			eventData.value_b                   = valueB;

			state->eventCallback(state->eventUserdata, &eventData);
		}

		mumble_sdk_status_t loadMetaConfiguration(ServerState *state) {
			if (!state) {
				return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
			}

			if (!Meta::mp) {
				Meta::mp = std::make_unique< MetaParams >();
			}

			QString iniPath = state->iniFilePath.empty() ? QString() : QString::fromUtf8(state->iniFilePath.c_str());
			Meta::mp->read(iniPath);

			if (!state->configOverrides.empty()) {
				for (const auto &[key, value] : state->configOverrides) {
					Meta::mp->qsSettings->setValue(QString::fromUtf8(key.c_str()), QString::fromUtf8(value.c_str()));
				}
				Meta::mp->qsSettings->sync();
				Meta::mp->read(Meta::mp->qsAbsSettingsFilePath);
			}

			return MUMBLE_SDK_STATUS_OK;
		}

		mumble_sdk_status_t ensureServerStarted(ServerState *state) {
			if (!state) {
				return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
			}

			if (state->started && meta) {
				return MUMBLE_SDK_STATUS_OK;
			}

			MumbleSSL::initialize();

			mumble_sdk_status_t status = loadMetaConfiguration(state);
			if (status != MUMBLE_SDK_STATUS_OK) {
				return status;
			}

			delete meta;
			meta = nullptr;

			meta = new Meta(Meta::getConnectionParameter());
			meta->initPBKDF2IterationCount();
			meta->getOSInfo();

			QObject::connect(meta, &Meta::started, meta, [state](Server *server) {
				emitServerEvent(state, MUMBLE_SDK_SERVER_EVENT_VIRTUAL_SERVER_STARTED, QString(), QString(),
								static_cast< std::int64_t >(server->iServerNum));
			});
			QObject::connect(meta, &Meta::stopped, meta, [state](Server *server) {
				emitServerEvent(state, MUMBLE_SDK_SERVER_EVENT_VIRTUAL_SERVER_STOPPED, QString(), QString(),
								static_cast< std::int64_t >(server->iServerNum));
			});

			meta->bootAll(Meta::getConnectionParameter(), true);
			state->started = true;
			emitServerEvent(state, MUMBLE_SDK_SERVER_EVENT_STARTED);

			return MUMBLE_SDK_STATUS_OK;
		}

		void stopServer(ServerState *state) {
			if (!state || !state->started) {
				return;
			}

			if (meta) {
				meta->killAll();
				delete meta;
				meta = nullptr;
			}

			Meta::mp.reset();
			delete qfLog;
			qfLog = nullptr;

			state->started = false;
			emitServerEvent(state, MUMBLE_SDK_SERVER_EVENT_STOPPED);
		}
	} // namespace
} // namespace Internal
} // namespace SDK
} // namespace Mumble

extern "C" {

mumble_sdk_status_t mumble_sdk_server_create(mumble_sdk_runtime_t *runtime, mumble_sdk_server_t **out_server) {
	if (!runtime || !runtime->state || !out_server) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	if (runtime->state->server) {
		return MUMBLE_SDK_STATUS_ALREADY_EXISTS;
	}

	auto serverState      = std::make_unique< Mumble::SDK::Internal::ServerState >();
	serverState->runtime  = runtime->state;
	runtime->state->serverCount++;

	mumble_sdk_server_t *server = new (std::nothrow) mumble_sdk_server_t();
	if (!server) {
		runtime->state->serverCount--;
		return MUMBLE_SDK_STATUS_INTERNAL_ERROR;
	}

	server->state          = serverState.get();
	runtime->state->server = std::move(serverState);
	*out_server            = server;
	return MUMBLE_SDK_STATUS_OK;
}

void mumble_sdk_server_destroy(mumble_sdk_server_t *server) {
	if (!server || !server->state) {
		return;
	}

	Mumble::SDK::Internal::RuntimeState *runtime = server->state->runtime;
	Mumble::SDK::Internal::invoke(runtime, [state = server->state]() {
		Mumble::SDK::Internal::stopServer(state);
	});

	runtime->server.reset();
	runtime->serverCount--;
	Mumble::SDK::Internal::maybeStopRuntime(runtime);
	delete server;
}

mumble_sdk_status_t mumble_sdk_server_set_event_callback(mumble_sdk_server_t *server,
														 mumble_sdk_server_event_callback_t callback, void *userdata) {
	if (!server || !server->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	server->state->eventCallback = callback;
	server->state->eventUserdata = userdata;
	return MUMBLE_SDK_STATUS_OK;
}

mumble_sdk_status_t mumble_sdk_server_set_ini_file(mumble_sdk_server_t *server, const char *ini_file_path) {
	if (!server || !server->state || server->state->started) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	server->state->iniFilePath = ini_file_path ? ini_file_path : "";
	return MUMBLE_SDK_STATUS_OK;
}

mumble_sdk_status_t mumble_sdk_server_set_config(mumble_sdk_server_t *server, const char *key, const char *value) {
	if (!server || !server->state || !key || !value || server->state->started) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	server->state->configOverrides[std::string(key)] = std::string(value);
	return MUMBLE_SDK_STATUS_OK;
}

mumble_sdk_status_t mumble_sdk_server_get_config(mumble_sdk_server_t *server, const char *key, char **out_value) {
	if (!server || !server->state || !key || !out_value) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime, [state = server->state, keyString = std::string(key),
																  out_value]() {
		mumble_sdk_status_t status = Mumble::SDK::Internal::loadMetaConfiguration(state);
		if (status != MUMBLE_SDK_STATUS_OK) {
			return status;
		}

		QString value = Meta::mp->qmConfig.value(QString::fromUtf8(keyString.c_str()));
		if (value.isNull()) {
			*out_value = nullptr;
			return MUMBLE_SDK_STATUS_NOT_FOUND;
		}

		*out_value = Mumble::SDK::Internal::duplicateUtf8(value);
		return *out_value ? MUMBLE_SDK_STATUS_OK : MUMBLE_SDK_STATUS_INTERNAL_ERROR;
	});
}

mumble_sdk_status_t mumble_sdk_server_start(mumble_sdk_server_t *server) {
	if (!server || !server->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime, [state = server->state]() {
		return Mumble::SDK::Internal::ensureServerStarted(state);
	});
}

mumble_sdk_status_t mumble_sdk_server_stop(mumble_sdk_server_t *server) {
	if (!server || !server->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime, [state = server->state]() {
		Mumble::SDK::Internal::stopServer(state);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_server_boot_virtual_server(mumble_sdk_server_t *server, uint32_t server_id) {
	if (!server || !server->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime, [state = server->state, server_id]() {
		mumble_sdk_status_t status = Mumble::SDK::Internal::ensureServerStarted(state);
		if (status != MUMBLE_SDK_STATUS_OK) {
			return status;
		}
		return meta->boot(Meta::getConnectionParameter(), server_id) ? MUMBLE_SDK_STATUS_OK
																	 : MUMBLE_SDK_STATUS_INTERNAL_ERROR;
	});
}

mumble_sdk_status_t mumble_sdk_server_kill_virtual_server(mumble_sdk_server_t *server, uint32_t server_id) {
	if (!server || !server->state) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime, [server_id]() {
		if (!meta) {
			return MUMBLE_SDK_STATUS_BUSY;
		}
		meta->kill(server_id);
		return MUMBLE_SDK_STATUS_OK;
	});
}

mumble_sdk_status_t mumble_sdk_server_set_superuser_password(mumble_sdk_server_t *server, uint32_t server_id,
															 const char *password) {
	if (!server || !server->state || !password) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime,
										 [state = server->state, server_id, passwordString = std::string(password)]() {
											 mumble_sdk_status_t status = Mumble::SDK::Internal::loadMetaConfiguration(state);
											 if (status != MUMBLE_SDK_STATUS_OK) {
												 return status;
											 }

											 if (meta) {
												 meta->dbWrapper.setSuperUserPassword(server_id, passwordString);
											 } else {
												 DBWrapper wrapper(Meta::getConnectionParameter());
												 wrapper.setSuperUserPassword(server_id, passwordString);
											 }
											 return MUMBLE_SDK_STATUS_OK;
										 });
}

mumble_sdk_status_t mumble_sdk_server_export_db_json(mumble_sdk_server_t *server, const char *output_path) {
	if (!server || !server->state || !output_path) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime,
										 [state = server->state, outputPath = std::string(output_path)]() {
											 mumble_sdk_status_t status = Mumble::SDK::Internal::loadMetaConfiguration(state);
											 if (status != MUMBLE_SDK_STATUS_OK) {
												 return status;
											 }

											 std::ofstream output(outputPath);
											 if (!output.is_open()) {
												 return MUMBLE_SDK_STATUS_NOT_FOUND;
											 }

											 if (meta) {
												 output << meta->dbWrapper.exportDBToJSON().dump(2);
											 } else {
												 DBWrapper wrapper(Meta::getConnectionParameter());
												 output << wrapper.exportDBToJSON().dump(2);
											 }
											 return MUMBLE_SDK_STATUS_OK;
										 });
}

mumble_sdk_status_t mumble_sdk_server_import_db_json(mumble_sdk_server_t *server, const char *input_path) {
	if (!server || !server->state || !input_path) {
		return MUMBLE_SDK_STATUS_INVALID_ARGUMENT;
	}

	return Mumble::SDK::Internal::invoke(server->state->runtime,
										 [state = server->state, inputPath = std::string(input_path)]() {
											 mumble_sdk_status_t status = Mumble::SDK::Internal::loadMetaConfiguration(state);
											 if (status != MUMBLE_SDK_STATUS_OK) {
												 return status;
											 }

											 std::ifstream input(inputPath);
											 if (!input.is_open()) {
												 return MUMBLE_SDK_STATUS_NOT_FOUND;
											 }

											 nlohmann::json json;
											 input >> json;

											 if (meta) {
												 meta->dbWrapper.importFromJSON(json, true);
											 } else {
												 DBWrapper wrapper(Meta::getConnectionParameter());
												 wrapper.importFromJSON(json, true);
											 }
											 return MUMBLE_SDK_STATUS_OK;
										 });
}

} // extern "C"
