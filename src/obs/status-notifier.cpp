#include "status-notifier.hpp"
#include "plugin-config.hpp"

#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QMainWindow>
#include <QMetaObject>
#include <QStatusBar>
#include <QCoreApplication>
#include <QEvent>

StatusNotifier &StatusNotifier::instance()
{
	static StatusNotifier inst;
	return inst;
}

StatusNotifier::StatusNotifier()
{
	connect(
		this, &StatusNotifier::notificationRequested, this,
		[](const QString &msg, int timeout) {
			if (!StatusNotifier::instance().enabled_ ||
			    !PluginConfig::instance().show_status_bar_notification) {
				return;
			}

			auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			if (main_win && main_win->statusBar()) {
				main_win->statusBar()->showMessage(msg, timeout);
			}
		},
		Qt::QueuedConnection);
}

void StatusNotifier::initialize()
{
	enabled_ = true;
}

void StatusNotifier::shutdown()
{
	enabled_ = false;
	QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
}

void StatusNotifier::notify(const std::string &message, int timeout_ms)
{
	obs_log(LOG_INFO, "[Timestamp Memo] %s", message.c_str());
	emit notificationRequested(QString::fromStdString(message), timeout_ms);
}
