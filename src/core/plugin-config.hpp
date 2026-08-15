#pragma once

#include "memo-marker.hpp"
#include <string>
#include <vector>

struct AutoExportConfig {
	bool json = true;
	bool csv = false;
	bool edl = false;
	bool srt = false;
	bool xml = false;
};

class PluginConfig {
public:
	static PluginConfig &instance();

	void load();
	void save();

	std::vector<MarkerTypeConfig> marker_types;
	AutoExportConfig auto_export;
	bool show_status_bar_notification = true;
	bool keep_tmp_cache_on_crash = true;

private:
	PluginConfig();
	void set_defaults();
};
