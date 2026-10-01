#pragma once

#include "timecode-helper.hpp"

#include <string>
#include <atomic>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QObject>

class ObsBridge : public QObject {
	Q_OBJECT

public:
	static ObsBridge &instance();

	void initialize();
	void shutdown();

	bool is_recording() const;
	bool is_paused() const;
	uint64_t get_current_record_ms() const;
	uint64_t get_current_record_frames() const;
	std::string get_current_record_file_path() const;
	VideoFrameRate get_current_frame_rate() const;
	void get_video_dimension(uint32_t &width, uint32_t &height) const;

signals:
	void obsRecordingStarted();
	void obsRecordingPaused();
	void obsRecordingUnpaused();
	void obsRecordingStopped();
	void obsRecordingFileChanged(const QString &path, uint64_t frame_offset);

	void obsTriggerMarker1();
	void obsTriggerMarker2();
	void obsTriggerMarker3();
	void obsTriggerMarker4();
	void obsFocusMemoRequested();

private:
	ObsBridge();
	~ObsBridge();

	static void on_frontend_event(enum obs_frontend_event event, void *private_data);
	static void on_save(obs_data_t *save_data, bool saving, void *private_data);
	static void on_file_changed(void *data, calldata_t *params);
	void attach_recording_output();
	void detach_recording_output();

	static void on_hotkey_marker_1(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_marker_2(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_marker_3(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_marker_4(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void on_hotkey_focus_memo(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	bool initialized_ = false;
	obs_output_t *recording_output_ = nullptr;
	std::string recording_path_;
	std::atomic<uint64_t> output_generation_{0};

	obs_hotkey_id hotkey_marker_1_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_marker_2_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_marker_3_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_marker_4_id_ = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hotkey_focus_memo_id_ = OBS_INVALID_HOTKEY_ID;
};
