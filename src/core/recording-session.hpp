#pragma once
#include "memo-marker.hpp"
#include <string>
#include <vector>
struct SessionMetadata {
	std::string video_path, session_id, started_at;
	VideoFrameRate fps{60, 1};
	uint32_t width = 1920, height = 1080;
};
// Value document: no OBS, Qt, clock, settings or disk dependencies.
class RecordingSession {
public:
	void replace(SessionMetadata metadata, std::vector<MemoMarker> markers = {});
	MemoMarker add_marker(uint64_t ms, int type, const std::string &label, const std::string &color,
			      const std::string &comment, bool paused, std::string created_at = "");
	MemoMarker add_marker_at_frame(uint64_t frame, int type, const std::string &label, const std::string &color,
				       const std::string &comment, bool paused, std::string created_at = "");
	bool update_marker(uint32_t id, const std::string &label, const std::string &color, const std::string &comment,
			   int type = -1);
	bool delete_marker(uint32_t id);
	void clear_markers() { markers_.clear(); }
	void set_video_path(const std::string &path) { metadata_.video_path = path; }
	const SessionMetadata &metadata() const { return metadata_; }
	const std::string &video_path() const { return metadata_.video_path; }
	const std::string &session_id() const { return metadata_.session_id; }
	const std::string &started_at() const { return metadata_.started_at; }
	VideoFrameRate frame_rate() const { return metadata_.fps; }
	uint32_t width() const { return metadata_.width; }
	uint32_t height() const { return metadata_.height; }
	const std::vector<MemoMarker> &get_markers() const { return markers_; }

private:
	SessionMetadata metadata_;
	std::vector<MemoMarker> markers_;
	uint64_t next_marker_id_ = 1;
};
