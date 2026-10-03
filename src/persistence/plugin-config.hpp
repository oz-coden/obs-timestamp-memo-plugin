#pragma once
#include "plugin-settings.hpp"
#include <QObject>
#include <QSettings>
#include <optional>
#include <string>
class PluginConfig : public QObject {
	Q_OBJECT
public:
	explicit PluginConfig(QSettings::Format format = QSettings::defaultFormat()) : format_(format) {}
	explicit PluginConfig(std::string file_path, QSettings::Format legacy_format = QSettings::defaultFormat())
		: format_(legacy_format),
		  file_path_(std::move(file_path))
	{
	}
	bool load();
	bool save(const PluginSettings &candidate);
	const PluginSettings &values() const { return values_; }
signals:
	void configChanged();

private:
	PluginSettings values_;
	QSettings::Format format_;
	std::optional<std::string> file_path_;
};
