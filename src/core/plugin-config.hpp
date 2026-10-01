#pragma once

#include "memo-marker.hpp"
#include <string>
#include <vector>
#include <QObject>

struct AutoExportConfig {
	bool json = true;
	bool csv = false;
	bool edl = false;
	bool srt = false;
	bool vtt = false;
	bool youtube = false;
	bool markdown = false;
	bool xml = false;
};

class PluginConfig : public QObject {
	Q_OBJECT

public:
	static PluginConfig &instance();

	void load();
	bool save();

	std::vector<MarkerTypeConfig> marker_types;
	AutoExportConfig auto_export;
	bool show_status_bar_notification = true;
	bool keep_tmp_cache_on_crash = true;

signals:
	void configChanged();

private:
	PluginConfig();
	void set_defaults();
};
