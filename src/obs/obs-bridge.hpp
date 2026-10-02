#pragma once

#include "recording-gateway.hpp"
#include <array>

#include <string>
#include <atomic>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QObject>

class ObsBridge : public RecordingGateway {
	Q_OBJECT

public:
	ObsBridge();
	~ObsBridge() override;
	RecordingSnapshot snapshot() const override;
	void drain_pending_events() override;

	std::string recovery_cache_directory() const;
	void initialize() override;
	void shutdown() override;

private:
	bool is_recording() const;
	bool is_paused() const;
	uint64_t get_current_record_frames() const;
	std::string get_current_record_file_path() const;
	void refresh_metadata(obs_encoder_t *encoder) const;
	struct RecordingMetadata {
		VideoFrameRate fps;
		uint32_t width = 1920, height = 1080;
	};
	mutable RecordingMetadata metadata_;
	mutable obs_encoder_t *metadata_encoder_ = nullptr; // Identity only; borrowed from the retained output.
	mutable bool metadata_valid_ = false;

private:
	static void on_frontend_event(enum obs_frontend_event event, void *private_data);
	static void on_save(obs_data_t *save_data, bool saving, void *private_data);
	static void on_file_changed(void *data, calldata_t *params);
	void attach_recording_output();
	void detach_recording_output();

	struct HotkeyBinding {
		ObsBridge *owner;
		int index;
	};
	static void on_hotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed);
	std::array<HotkeyBinding, 5> hotkey_bindings_;
	std::array<obs_hotkey_id, 5> hotkey_ids_;
	bool initialized_ = false;
	obs_output_t *recording_output_ = nullptr;
	std::string recording_path_;
	std::atomic<uint64_t> output_generation_{0};
};
