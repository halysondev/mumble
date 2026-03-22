// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_SERVER_SERVERAPPLICATION_H_
#define MUMBLE_SERVER_SERVERAPPLICATION_H_

#include <QtGlobal>

#if defined(Q_OS_WIN) && !defined(MUMBLE_SERVER_NO_GUI)
#	include <QApplication>
#else
#	include <QCoreApplication>
#endif

class QObject;
class QEvent;

#if defined(Q_OS_WIN) && !defined(MUMBLE_SERVER_NO_GUI)
class ServerApplication : public QApplication {
#else
class ServerApplication : public QCoreApplication {
#endif
	Q_OBJECT

public:
#if defined(Q_OS_WIN) && !defined(MUMBLE_SERVER_NO_GUI)
	using QApplication::QApplication;
#else
	using QCoreApplication::QCoreApplication;
#endif
	~ServerApplication();

	/**
	 * We override this function that is responsible for performing the actual
	 * signal-slot-calling in order to be able to catch exceptions in there.
	 */
	bool notify(QObject *receiver, QEvent *event) override;
};

#endif // MUMBLE_SERVER_SERVERAPPLICATION_H_
