#include "status-notifier.hpp"
#include "plugin-config.hpp"
#include <plugin-support.h>
#include <QMainWindow>
#include <QCoreApplication>
#include <QEvent>
StatusNotifier::StatusNotifier(QMainWindow &window, const PluginConfig &config)
	: status_bar_(window.statusBar()),
	  config_(config)
{
}
void StatusNotifier::shutdown()
{
	enabled_ = false;
	QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
}
void StatusNotifier::notify(const QString &message, int timeout, NotificationSeverity severity)
{
	if (!enabled_)
		return;
	obs_log(severity == NotificationSeverity::Info
			? LOG_INFO
			: (severity == NotificationSeverity::Error ? LOG_ERROR : LOG_WARNING),
		"[Timestamp Memo] %s", message.toUtf8().constData());
	if (status_bar_ && (severity != NotificationSeverity::Info || config_.values().show_status_bar_notification))
		status_bar_->showMessage(message, timeout);
}
