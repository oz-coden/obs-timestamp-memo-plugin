#include "recording-session.hpp"
#include <algorithm>
#include <unordered_set>
#include <utility>
#include <limits>
void RecordingSession::replace(SessionMetadata metadata, std::vector<MemoMarker> markers)
{
	metadata_ = std::move(metadata);
	if (!metadata_.fps.valid())
		metadata_.fps = {60, 1};
	markers_ = std::move(markers);
	next_marker_id_ = 1;
	std::unordered_set<uint32_t> seen;
	for (auto &m : markers_) {
		if (!m.id || seen.count(m.id))
			m.id = static_cast<uint32_t>(next_marker_id_);
		seen.insert(m.id);
		next_marker_id_ = std::max(next_marker_id_, static_cast<uint64_t>(m.id) + 1);
		m.timecode_ndf = TimecodeHelper::frame_index_to_smpte(m.frame_index, metadata_.fps, true);
		m.timecode_df = TimecodeHelper::frame_index_to_smpte(m.frame_index, metadata_.fps);
	}
}
MemoMarker RecordingSession::add_marker(uint64_t ms, int type, const std::string &label, const std::string &color,
					const std::string &comment, bool paused, std::string created_at)
{
	if (next_marker_id_ > std::numeric_limits<uint32_t>::max())
		return {};
	auto m = MemoMarker::create(static_cast<uint32_t>(next_marker_id_++), ms, metadata_.fps, type, label, color,
				    comment, paused, std::move(created_at));
	markers_.push_back(m);
	return m;
}
MemoMarker RecordingSession::add_marker_at_frame(uint64_t frame, int type, const std::string &label,
						 const std::string &color, const std::string &comment, bool paused,
						 std::string created_at)
{
	auto m = add_marker(TimecodeHelper::frame_index_to_ms(frame, metadata_.fps), type, label, color, comment,
			    paused, std::move(created_at));
	if (!m.id)
		return m;
	m.frame_index = frame;
	m.timecode_ndf = TimecodeHelper::frame_index_to_smpte(frame, metadata_.fps, true);
	m.timecode_df = TimecodeHelper::frame_index_to_smpte(frame, metadata_.fps);
	markers_.back() = m;
	return m;
}
bool RecordingSession::update_marker(uint32_t id, const std::string &label, const std::string &color,
				     const std::string &comment, int type)
{
	for (auto &m : markers_) {
		if (m.id != id)
			continue;
		m.label = label;
		m.color = color;
		m.comment = comment;
		if (type >= 0 && type < 4)
			m.type_index = type;
		return true;
	}
	return false;
}
bool RecordingSession::delete_marker(uint32_t id)
{
	auto it = std::find_if(markers_.begin(), markers_.end(), [id](const auto &m) { return m.id == id; });
	if (it == markers_.end())
		return false;
	markers_.erase(it);
	return true;
}
