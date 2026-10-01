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
void StatusNotifier::notify(const QString &message, int timeout)
{
	if (!enabled_)
		return;
	obs_log(LOG_INFO, "[Timestamp Memo] %s", message.toUtf8().constData());
	if (status_bar_ && config_.values().show_status_bar_notification)
		status_bar_->showMessage(message, timeout);
}
