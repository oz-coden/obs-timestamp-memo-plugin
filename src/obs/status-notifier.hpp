#pragma once
#include <QObject>
#include <QPointer>
#include <QStatusBar>
#include "notification.hpp"
class QMainWindow;
class PluginConfig;
class StatusNotifier : public QObject {
	Q_OBJECT
public:
	StatusNotifier(QMainWindow &window, const PluginConfig &config);
	void initialize() { enabled_ = true; }
	void shutdown();
public slots:
	void notify(const QString &message, int timeout = 3000,
		    NotificationSeverity severity = NotificationSeverity::Info);

private:
	QPointer<QStatusBar> status_bar_;
	const PluginConfig &config_;
	bool enabled_ = false;
};
