#pragma once
#include <array>
#include <string>
struct MarkerTypeConfig {
	int type_index = 0;
	std::string label = "Marker 1";
	std::string color = "#3498db";
	std::string hotkey_name = "marker_1";
};

struct AutoExportConfig {
	bool json = true, csv = false, edl = false, srt = false, vtt = false, youtube = false, markdown = false,
	     xml = false;
};
struct PluginSettings {
	std::array<MarkerTypeConfig, 4> marker_types{{{0, "Marker 1", "#3498db", "hotkey_marker_1"},
						      {1, "Chapter", "#2ecc71", "hotkey_marker_2"},
						      {2, "Highlight", "#f1c40f", "hotkey_marker_3"},
						      {3, "Cut / Edit", "#e74c3c", "hotkey_marker_4"}}};
	AutoExportConfig auto_export;
	bool show_status_bar_notification = true;
};
