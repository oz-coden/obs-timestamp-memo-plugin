#pragma once

#include "timecode-helper.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct MarkerTypeConfig {
	int type_index = 0;
	std::string label = "Marker 1";
	std::string color = "#3498db";
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

	static MemoMarker create(uint32_t id, uint64_t ms, const VideoFrameRate &fps, int type_index,
				 std::string_view label, std::string_view color, std::string_view comment,
				 bool is_paused, std::string created_at = "");

	std::string active_timecode(const VideoFrameRate &fps) const;
};
