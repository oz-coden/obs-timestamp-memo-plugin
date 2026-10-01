#include "memo-marker.hpp"
#include <utility>

MemoMarker MemoMarker::create(uint32_t id, uint64_t ms, const VideoFrameRate &fps, int type_index,
			      std::string_view label, std::string_view color, std::string_view comment, bool is_paused,
			      std::string created_at)
{
	MemoMarker m;
	m.id = id;
	m.timestamp_ms = ms;
	m.frame_index = TimecodeHelper::ms_to_frame_index(ms, fps);
	m.timecode_ndf = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps, true);
	m.timecode_df = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps, false);
	m.type_index = (type_index < 0 || type_index >= 4) ? 0 : type_index;
	m.label = label;
	m.color = color.empty() ? "#3498db" : color;
	m.comment = comment;
	m.created_at_utc = std::move(created_at);
	m.is_paused = is_paused;
	return m;
}

std::string MemoMarker::active_timecode(const VideoFrameRate &fps) const
{
	if (fps.is_drop_frame()) {
		return timecode_df;
	}
	return timecode_ndf;
}
