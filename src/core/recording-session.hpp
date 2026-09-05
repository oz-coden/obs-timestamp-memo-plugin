#pragma once

#include "memo-marker.hpp"
#include "timecode-helper.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <QFile>
#include <QJsonObject>
#include <QTextStream>

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

	bool is_active() const;
	std::string video_path() const;
	void set_video_path(const std::string &path);
	std::string session_id() const;
	std::string started_at() const;
	VideoFrameRate frame_rate() const;
	uint32_t width() const;
	uint32_t height() const;
	std::vector<MemoMarker> get_markers() const;

	QJsonObject to_json() const;

private:
	std::string get_cache_file_path() const;
	void flush_journal_entry(const QJsonObject &entry);
	void cleanup_cache_file(bool force = false);

	mutable std::recursive_mutex mutex_;
	bool active_ = false;
	std::string video_path_;
	std::string session_id_;
	std::string started_at_;
	VideoFrameRate fps_{60, 1};
	uint32_t width_ = 1920;
	uint32_t height_ = 1080;
	std::vector<MemoMarker> markers_;
	uint32_t next_marker_id_ = 1;

	std::unique_ptr<QFile> cache_file_;
	std::unique_ptr<QTextStream> cache_stream_;
	std::string cache_file_path_;
};
