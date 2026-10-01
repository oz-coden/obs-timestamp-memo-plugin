#include "obs-bridge.hpp"
#include <plugin-support.h>
#include <QCoreApplication>
#include <QEvent>
#include <limits>
#include <numeric>

ObsBridge::ObsBridge()
{
	hotkey_ids_.fill(OBS_INVALID_HOTKEY_ID);
	for (int i = 0; i < 5; ++i)
		hotkey_bindings_[i] = {this, i};
}

ObsBridge::~ObsBridge()
{
	shutdown();
}

void ObsBridge::initialize()
{
	if (initialized_)
		return;

	obs_frontend_add_event_callback(on_frontend_event, this);
	obs_frontend_add_save_callback(on_save, this);

	const char *names[] = {"obs_timestamp_memo_marker_1", "obs_timestamp_memo_marker_2",
			       "obs_timestamp_memo_marker_3", "obs_timestamp_memo_marker_4",
			       "obs_timestamp_memo_focus_input"};
	const char *descriptions[] = {"Timestamp Memo: Quick Marker 1", "Timestamp Memo: Quick Marker 2",
				      "Timestamp Memo: Quick Marker 3", "Timestamp Memo: Quick Marker 4",
				      "Timestamp Memo: Focus Memo Input"};
	for (int i = 0; i < 5; ++i)
		hotkey_ids_[i] =
			obs_hotkey_register_frontend(names[i], descriptions[i], on_hotkey, &hotkey_bindings_[i]);

	initialized_ = true;
	if (is_recording())
		attach_recording_output();
	obs_log(LOG_INFO, "[Timestamp Memo] ObsBridge initialized");
}

void ObsBridge::shutdown()
{
	if (!initialized_)
		return;
	QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
	initialized_ = false;
	detach_recording_output();
	QCoreApplication::removePostedEvents(this, QEvent::MetaCall);

	obs_frontend_remove_event_callback(on_frontend_event, this);
	obs_frontend_remove_save_callback(on_save, this);

	for (auto &id : hotkey_ids_) {
		if (id != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(id);
		id = OBS_INVALID_HOTKEY_ID;
	}
}

bool ObsBridge::is_recording() const
{
	return obs_frontend_recording_active();
}

bool ObsBridge::is_paused() const
{
	return obs_frontend_recording_paused();
}

uint64_t ObsBridge::get_current_record_frames() const
{
	auto *output = recording_output_;
	if (!output) {
		return 0;
	}

	int total_frames = obs_output_get_total_frames(output);

	return total_frames > 0 ? static_cast<uint64_t>(total_frames) : 0;
}

std::string ObsBridge::get_current_record_file_path() const
{
	if (!recording_path_.empty())
		return recording_path_;
	std::string file_path = "";
	auto *output = recording_output_;
	if (output) {
		obs_data_t *settings = obs_output_get_settings(output);
		if (settings) {
			const char *path = obs_data_get_string(settings, "path");
			if (!path || !*path) {
				path = obs_data_get_string(settings, "url");
			}
			if (!path || !*path) {
				path = obs_data_get_string(settings, "file");
			}
			if (path && *path) {
				file_path = path;
			}
			obs_data_release(settings);
		}
	}
	return file_path;
}

void ObsBridge::attach_recording_output()
{
	detach_recording_output();
	recording_path_.clear();
	// OBS has not constructed outputHandler during module loading. Acquire an
	// output only after recording becomes active, then retain it through STOPPED.
	recording_output_ = obs_frontend_get_recording_output();
	recording_path_ = get_current_record_file_path();
	if (recording_output_)
		signal_handler_connect(obs_output_get_signal_handler(recording_output_), "file_changed",
				       on_file_changed, this);
}

void ObsBridge::detach_recording_output()
{
	++output_generation_;
	if (recording_output_) {
		signal_handler_disconnect(obs_output_get_signal_handler(recording_output_), "file_changed",
					  on_file_changed, this);
		obs_output_release(recording_output_);
		recording_output_ = nullptr;
	}
}

void ObsBridge::on_file_changed(void *data, calldata_t *params)
{
	auto *self = static_cast<ObsBridge *>(data);
	const char *path = calldata_string(params, "next_file");
	if (!self || !path || !*path)
		return;
	QString next_path = QString::fromUtf8(path);
	int total = obs_output_get_total_frames(self->recording_output_);
	uint64_t frames = total > 0 ? static_cast<uint64_t>(total) : 0;
	uint64_t generation = self->output_generation_.load();
	QMetaObject::invokeMethod(
		self,
		[self, next_path, frames, generation]() {
			if (!self->initialized_ || generation != self->output_generation_.load())
				return;
			self->recording_path_ = next_path.toStdString();
			emit self->recordingFileChanged(next_path, frames);
		},
		Qt::QueuedConnection);
}

VideoFrameRate ObsBridge::get_current_frame_rate() const
{
	struct obs_video_info ovi{};
	VideoFrameRate rate{60, 1};
	if (obs_get_video_info(&ovi) && ovi.fps_num && ovi.fps_den)
		rate = VideoFrameRate{ovi.fps_num, ovi.fps_den};
	auto *output = recording_output_;
	if (output) {
		obs_encoder_t *encoder = obs_output_get_video_encoder(output);
		uint32_t divisor = encoder ? obs_encoder_get_frame_rate_divisor(encoder) : 1;
		if (divisor > 1) {
			uint32_t common = std::gcd(rate.num, divisor);
			divisor /= common;
			if (rate.den <= std::numeric_limits<uint32_t>::max() / divisor) {
				rate.num /= common;
				rate.den *= divisor;
			}
		}
	}
	return rate;
}

void ObsBridge::get_video_dimension(uint32_t &width, uint32_t &height) const
{
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		width = ovi.output_width;
		height = ovi.output_height;
	} else {
		width = 1920;
		height = 1080;
	}
	auto *output = recording_output_;
	if (output) {
		obs_encoder_t *encoder = obs_output_get_video_encoder(output);
		if (encoder && obs_encoder_get_width(encoder) && obs_encoder_get_height(encoder)) {
			width = obs_encoder_get_width(encoder);
			height = obs_encoder_get_height(encoder);
		}
	}
}

void ObsBridge::on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	auto *self = static_cast<ObsBridge *>(private_data);
	if (!self)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		self->attach_recording_output();
		emit self->recordingStarted();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		emit self->recordingPaused();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		emit self->recordingUnpaused();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		QCoreApplication::sendPostedEvents(self, QEvent::MetaCall);
		emit self->recordingStopped();
		self->detach_recording_output();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		self->shutdown();
		break;
	default:
		break;
	}
}

void ObsBridge::on_save(obs_data_t *save_data, bool saving, void *private_data)
{
	auto *self = static_cast<ObsBridge *>(private_data);
	if (!self || !save_data)
		return;

	if (saving) {
		obs_data_array_t *hotkey_arr_1 = obs_hotkey_save(self->hotkey_ids_[0]);
		obs_data_array_t *hotkey_arr_2 = obs_hotkey_save(self->hotkey_ids_[1]);
		obs_data_array_t *hotkey_arr_3 = obs_hotkey_save(self->hotkey_ids_[2]);
		obs_data_array_t *hotkey_arr_4 = obs_hotkey_save(self->hotkey_ids_[3]);
		obs_data_array_t *hotkey_arr_focus = obs_hotkey_save(self->hotkey_ids_[4]);

		obs_data_set_array(save_data, "hotkey_marker_1", hotkey_arr_1);
		obs_data_set_array(save_data, "hotkey_marker_2", hotkey_arr_2);
		obs_data_set_array(save_data, "hotkey_marker_3", hotkey_arr_3);
		obs_data_set_array(save_data, "hotkey_marker_4", hotkey_arr_4);
		obs_data_set_array(save_data, "hotkey_focus_memo", hotkey_arr_focus);

		obs_data_array_release(hotkey_arr_1);
		obs_data_array_release(hotkey_arr_2);
		obs_data_array_release(hotkey_arr_3);
		obs_data_array_release(hotkey_arr_4);
		obs_data_array_release(hotkey_arr_focus);
	} else {
		obs_data_array_t *hotkey_arr_1 = obs_data_get_array(save_data, "hotkey_marker_1");
		obs_data_array_t *hotkey_arr_2 = obs_data_get_array(save_data, "hotkey_marker_2");
		obs_data_array_t *hotkey_arr_3 = obs_data_get_array(save_data, "hotkey_marker_3");
		obs_data_array_t *hotkey_arr_4 = obs_data_get_array(save_data, "hotkey_marker_4");
		obs_data_array_t *hotkey_arr_focus = obs_data_get_array(save_data, "hotkey_focus_memo");

		obs_hotkey_load(self->hotkey_ids_[0], hotkey_arr_1);
		obs_hotkey_load(self->hotkey_ids_[1], hotkey_arr_2);
		obs_hotkey_load(self->hotkey_ids_[2], hotkey_arr_3);
		obs_hotkey_load(self->hotkey_ids_[3], hotkey_arr_4);
		obs_hotkey_load(self->hotkey_ids_[4], hotkey_arr_focus);

		obs_data_array_release(hotkey_arr_1);
		obs_data_array_release(hotkey_arr_2);
		obs_data_array_release(hotkey_arr_3);
		obs_data_array_release(hotkey_arr_4);
		obs_data_array_release(hotkey_arr_focus);
	}
}

void ObsBridge::on_hotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed || !data)
		return;
	auto *binding = static_cast<HotkeyBinding *>(data);
	if (binding->index < 4)
		emit binding->owner->quickMarkerRequested(binding->index);
	else
		emit binding->owner->focusMemoRequested();
}

std::string ObsBridge::recovery_cache_directory() const
{
	char *path = obs_module_config_path("cache");
	if (!path)
		return "";
	std::string result = path;
	bfree(path);
	return result;
}

RecordingSnapshot ObsBridge::snapshot() const
{
	RecordingSnapshot value;
	if (!initialized_)
		return value;
	value.recording = is_recording();
	value.paused = value.recording && is_paused();
	if (!recording_output_)
		return value;
	value.total_frames = get_current_record_frames();
	value.path = get_current_record_file_path();
	value.fps = get_current_frame_rate();
	get_video_dimension(value.width, value.height);
	return value;
}
void ObsBridge::drain_pending_events()
{
	QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
}
