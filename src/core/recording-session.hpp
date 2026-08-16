#pragma once

#include "memo-marker.hpp"
#include "timecode-helper.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <QJsonObject>

class RecordingSession {
public:
	RecordingSession();
	~RecordingSession();

	bool start_session(const std::string &video_path, const VideoFrameRate &fps, uint32_t width, uint32_t height);
	bool stop_session(std::string *out_json_path = nullptr);

	static bool recover_from_cache(const std::string &cache_path, RecordingSession &out_session);

	bool load_from_json(const std::string &json_path);
	bool save_to_json(const std::string &target_json_path = "");

	MemoMarker add_marker(uint64_t ms, int type_index, const std::string &label, const std::string &color,
			      const std::string &comment, bool is_paused);

	bool update_marker(uint32_t marker_id, const std::string &label, const std::string &color,
			   const std::string &comment);

	bool delete_marker(uint32_t marker_id);
	void clear_markers();

	bool is_active() const { return active_; }
	std::string video_path() const { return video_path_; }
	std::string session_id() const { return session_id_; }
	std::string started_at() const { return started_at_; }
	VideoFrameRate frame_rate() const { return fps_; }
	uint32_t width() const { return width_; }
	uint32_t height() const { return height_; }
	std::vector<MemoMarker> get_markers() const;

	QJsonObject to_json() const;

private:
	std::string get_cache_file_path() const;
	void flush_journal_entry(const QJsonObject &entry);
	void cleanup_cache_file();

	mutable std::mutex mutex_;
	bool active_ = false;
	std::string video_path_;
	std::string session_id_;
	std::string started_at_;
	VideoFrameRate fps_{60, 1};
	uint32_t width_ = 1920;
	uint32_t height_ = 1080;
	std::vector<MemoMarker> markers_;
	uint32_t next_marker_id_ = 1;

	std::ofstream cache_stream_;
	std::string cache_file_path_;
};
