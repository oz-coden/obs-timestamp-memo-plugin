#pragma once

#include "timecode-helper.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <QJsonObject>
#include <QJsonArray>

struct MarkerTypeConfig {
	int type_index = 0;
	std::string label = "Marker 1";
	std::string color = "#3498db"; // Default Blue
	std::string hotkey_name = "marker_1";
};

class MemoMarker {
public:
	uint32_t id = 0;
	uint64_t timestamp_ms = 0;
	uint64_t frame_index = 0;
	std::string timecode_ndf;
	std::string timecode_df;
	int type_index = 0;
	std::string label;
	std::string color = "#3498db";
	std::string comment;
	std::string created_at_utc;
	bool is_paused = false;

	static MemoMarker create(uint32_t id, uint64_t ms, const VideoFrameRate &fps,
				  int type_index, const std::string &label, const std::string &color,
				  const std::string &comment, bool is_paused);

	QJsonObject to_json() const;
	static MemoMarker from_json(const QJsonObject &obj);

	std::string active_timecode(const VideoFrameRate &fps) const;
};
