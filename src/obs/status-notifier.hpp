#pragma once

#include <string>
#include <QObject>

class StatusNotifier : public QObject {
	Q_OBJECT

public:
	static StatusNotifier &instance();

	void notify(const std::string &message, int timeout_ms = 3000);

signals:
	void notificationRequested(const QString &message, int timeout_ms);

private:
	StatusNotifier();
};
