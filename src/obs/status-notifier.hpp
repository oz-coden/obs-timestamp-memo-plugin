#pragma once
#include <QObject>
#include <QPointer>
#include <QStatusBar>
class QMainWindow;
class PluginConfig;
class StatusNotifier : public QObject {
	Q_OBJECT
public:
	StatusNotifier(QMainWindow &window, const PluginConfig &config);
	void initialize() { enabled_ = true; }
	void shutdown();
public slots:
	void notify(const QString &message, int timeout = 3000);

private:
	QPointer<QStatusBar> status_bar_;
	const PluginConfig &config_;
	bool enabled_ = false;
};
