#pragma once
#include <array>
#include <string>
struct MarkerTypeConfig {
	std::string label = "Marker 1";
	std::string color = "#3498db";
};

struct AutoExportConfig {
	bool json = true, csv = false, edl = false, srt = false, vtt = false, youtube = false, markdown = false,
	     xml = false;
};
struct PluginSettings {
	std::array<MarkerTypeConfig, 4> marker_types{
		{{"Marker 1", "#3498db"}, {"Chapter", "#2ecc71"}, {"Highlight", "#f1c40f"}, {"Cut / Edit", "#e74c3c"}}};
	AutoExportConfig auto_export;
	bool show_status_bar_notification = true;
};
