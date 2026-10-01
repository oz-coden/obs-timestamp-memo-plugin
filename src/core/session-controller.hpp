#pragma once

#include "memo-marker.hpp"
#include "recording-session.hpp"
#include "recording-timeline.hpp"
#include "session-store.hpp"
#include "timecode-helper.hpp"

#include <memory>
#include <string>
#include <vector>
#include <QObject>

class SessionController : public QObject {
	Q_OBJECT

public:
	static SessionController &instance();

	void initialize();
	void shutdown();

	// Session inspection
	const RecordingSession &session() const { return session_; }
	bool is_recording() const;
	bool is_paused() const;
	uint64_t current_record_ms() const;
	VideoFrameRate current_frame_rate() const;
	std::string current_video_path() const;

	// User Actions
	bool trigger_quick_marker(int type_index, const std::string &comment = "");
	bool add_memo_marker(const std::string &comment, int type_index = 0);
	bool update_marker_comment(uint32_t id, const std::string &comment);
	bool update_marker_type(uint32_t id, int type_index);
	bool update_marker_data(uint32_t id, const std::string &label, const std::string &color,
				const std::string &comment);
	bool delete_marker(uint32_t id);
	void clear_markers();

	// Persistence / File ops
	bool load_from_json(const std::string &path);
	bool recover_from_cache(const std::string &cache_path);
	void save_current_session();

	// Auto export
	bool perform_auto_export(const std::string &base_video_path);

	// Periodic / Polling
	void check_recording_file_changed();

signals:
	void markerAdded(const MemoMarker &marker, int row);
	void markerUpdated(const MemoMarker &marker, int row);
	void markerRemoved(uint32_t id, int row);
	void markersReset(const std::vector<MemoMarker> &markers, const VideoFrameRate &fps);

	void sessionStarted(const QString &videoPath);
	void sessionPaused();
	void sessionResumed();
	void sessionStopped(const QString &jsonPath);
	void videoPathResolved(const QString &newVideoPath);

	void focusMemoInputRequested();

private:
	SessionController();
	~SessionController();

	void onRecordingStarted();
	void onRecordingPaused();
	void onRecordingUnpaused();
	void onRecordingStopped();
	void onRecordingFileSplit(const QString &path, uint64_t frame_offset);
	void persist_edits();
	void check_journal();

	RecordingSession session_;
	RecordingTimeline timeline_;
	SessionStore store_;
	void journal_update(uint32_t id);
	void start_document(const std::string &path, const VideoFrameRate &fps, uint32_t width, uint32_t height);
	bool initialized_ = false;
	bool journal_warning_shown_ = false;
};
