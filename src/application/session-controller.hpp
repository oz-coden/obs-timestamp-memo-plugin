#pragma once

#include "memo-marker.hpp"
#include "recording-session.hpp"
#include "recording-timeline.hpp"
#include "session-store.hpp"
#include "timecode-helper.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <QObject>
#include <QTimer>
#include "recording-gateway.hpp"
class PluginConfig;
class ExporterRegistry;

struct UnsavedDocument {
	uint64_t id = 0;
	std::string title, recovery_path, journal_path;
	std::optional<RecordingSession> memory;
};

class SessionController : public QObject {
	Q_OBJECT

public:
	SessionController(RecordingGateway &bridge, PluginConfig &config, SessionStore &store,
			  ExporterRegistry &exporters);
	~SessionController() override;
	void notify(const std::string &message, int timeout = 3000)
	{
		emit notificationRequested(QString::fromStdString(message), timeout);
	}
	QString document_title() const;

	void initialize();
	void shutdown();

	// Session inspection
	const RecordingSession &session() const { return session_; }
	bool is_recording() const;
	bool is_paused() const;
	uint64_t current_record_ms() const;
	std::string current_video_path() const;

	// User Actions
	bool trigger_quick_marker(int type_index, const std::string &comment = "");
	bool add_memo_marker(const std::string &comment, int type_index = 0);
	bool update_marker_type(uint32_t id, int type_index);
	bool update_marker_data(uint32_t id, const std::string &label, const std::string &color,
				const std::string &comment);
	bool delete_marker(uint32_t id);
	void clear_markers();

	// Persistence / File ops
	bool load_from_json(const std::string &path);
	bool recover_from_cache(const std::string &cache_path);
	const std::vector<UnsavedDocument> &unsaved_documents() const { return unsaved_documents_; }
	bool has_unsaved_current() const { return document_unsaved_ && (!is_recording() || capture_blocked_); }
	bool can_stamp() const { return is_recording() && !capture_blocked_; }
	bool read_unsaved_document(uint64_t id, RecordingSession &document) const;
	bool save_unsaved_document(uint64_t id, const std::string &path);
	bool save_current_document(const std::string &path);

	// Auto export
	bool perform_auto_export(const std::string &base_video_path);

	// Periodic / Polling
	void check_recording_file_changed();

signals:
	void notificationRequested(const QString &message, int timeout);
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
	void unsavedDocumentsChanged();

private:
	void onRecordingStarted();
	void onRecordingPaused();
	void onRecordingUnpaused();
	void onRecordingStopped();
	void onRecordingFileSplit(const QString &path, uint64_t frame_offset);
	void persist_edits();
	void check_journal();

	RecordingSession session_;
	RecordingTimeline timeline_;
	RecordingGateway &bridge_;
	PluginConfig &config_;
	SessionStore &store_;
	ExporterRegistry &exporters_;
	QTimer poll_timer_;
	std::string history_path_;
	bool recovered_ = false;
	bool apply_marker_update(const MemoMarker &candidate);
	void journal_update(uint32_t id);
	void start_document(const std::string &path, const VideoFrameRate &fps, uint32_t width, uint32_t height);
	bool initialized_ = false;
	bool journal_warning_shown_ = false;
	std::vector<UnsavedDocument> unsaved_documents_;
	uint64_t next_unsaved_id_ = 1;
	bool document_unsaved_ = false, capture_blocked_ = false;
	bool journal_current_ = false;
	RecordingSnapshot pending_capture_;
	bool preserve_current_document();
	void resume_capture();
};
