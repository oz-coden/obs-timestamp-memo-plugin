#pragma once

#include "memo-marker.hpp"
#include "recording-session.hpp"
#include "timecode-helper.hpp"

#include <memory>
#include <string>
#include <vector>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QObject>

class ObsBridge : public QObject {
	Q_OBJECT

public:
	static ObsBridge &instance();

	void initialize();
	void shutdown();

	bool trigger_marker(int type_index, const std::string &custom_comment = "");
	bool add_memo_marker(const std::string &text, int type_index = 0);

	RecordingSession &session()
	{
		return session_;
	}

	bool is_recording() const;
	bool is_paused() const;
	uint64_t get_current_record_ms() const;
	std::string get_current_record_file_path() const;
	VideoFrameRate get_current_frame_rate() const;
	void get_video_dimension(uint32_t &width, uint32_t &height) const;

	void perform_auto_export(const std::string &base_video_path);

signals:
	void markerAdded(const MemoMarker &marker);
	void markerUpdated(uint32_t id);
	void markerDeleted(uint32_t id);
	void markersCleared();
	void recordingStarted(const QString &videoPath);
	void recordingPaused();
	void recordingUnpaused();
	void recordingStopped(const QString &jsonPath);
	void recordingFileChanged(const QString &newVideoPath);
	void focusMemoInputRequested();

private:
	ObsBridge();
	~ObsBridge();

	static void on_frontend_event(enum obs_frontend_event event, void *private_data);
	static void on_save(obs_data_t *save_data, bool saving, void *private_data);

	static void on_hotkey_marker_1(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_marker_2(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_marker_3(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_marker_4(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_focus_memo(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	void handle_recording_started();
	void handle_recording_paused();
	void handle_recording_unpaused();
	void handle_recording_stopped();
	void handle_recording_file_changed();

	RecordingSession session_;
	bool initialized_ = false;

	obs_hotkey_id hotkey_marker_1_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_marker_2_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_marker_3_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_marker_4_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_focus_memo_id_ = OBS_INVALID_HOTKEY_ID;
};
