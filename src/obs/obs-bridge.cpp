#include "obs-bridge.hpp"
#include <plugin-support.h>
#include <QCoreApplication>
#include <QEvent>
#include <limits>
#include <numeric>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>

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
		hotkey_ids_[i] = obs_hotkey_register_frontend(
			names[i], QCoreApplication::translate("TimestampMemo", descriptions[i]).toUtf8().constData(),
			on_hotkey, &hotkey_bindings_[i]);

	hotkeys_path_ = config_path("hotkeys.json");
	hotkeys_checked_ = load_global_hotkeys();
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
	if (hotkeys_writable_ && !save_global_hotkeys())
		emit integrationWarning("Could not save plugin hotkeys. Previous hotkey data was retained.");
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
	if (recording_output_) {
		refresh_metadata(obs_output_get_video_encoder(recording_output_));
		const auto flags = obs_output_get_flags(recording_output_);
		const bool timed_packets = (flags & OBS_OUTPUT_ENCODED) && (flags & OBS_OUTPUT_AUDIO);
		{
			std::lock_guard lock(clock_mutex_);
			clock_.reset(obs_get_video_frame_time(), !timed_packets);
			clock_fps_ = metadata_.fps;
			keyframe_ns_.reset();
		}
		obs_output_add_packet_callback(recording_output_, on_packet, this);
		if (!timed_packets)
			emit integrationWarning(
				"This recording output has no timed packet callbacks. Marker time uses a recording-start video-clock estimate.");
		signal_handler_connect(obs_output_get_signal_handler(recording_output_), "file_changed",
				       on_file_changed, this);
	}
}

void ObsBridge::detach_recording_output()
{
	++output_generation_;
	metadata_valid_ = false;
	metadata_encoder_ = nullptr;
	if (recording_output_) {
		obs_output_remove_packet_callback(recording_output_, on_packet, this);
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
	uint64_t frames;
	{
		std::lock_guard lock(self->clock_mutex_);
		// FFmpeg splits on a video keyframe. file_changed does not expose its
		// PTS; use the most recent observed keyframe, or a clock estimate for
		// outputs without timed packets. Exact custom-muxer boundaries need
		// real-file verification (see the manual validation guide).
		const auto boundary = self->keyframe_ns_ ? *self->keyframe_ns_
							 : self->clock_.time(obs_get_video_frame_time());
		frames = TimecodeHelper::ns_to_frame_index(boundary, self->clock_fps_);
	}
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

void ObsBridge::on_packet(obs_output_t *, encoder_packet *packet, encoder_packet_time *timing, void *data)
{
	auto *self = static_cast<ObsBridge *>(data);
	if (!packet || packet->type != OBS_ENCODER_VIDEO || packet->track_idx != 0)
		return;
	std::lock_guard lock(self->clock_mutex_);
	if (packet->keyframe) {
		const auto pts = PresentationClock::pts_to_ns(packet->pts, packet->timebase_den);
		if (pts && (!self->keyframe_ns_ || *pts >= *self->keyframe_ns_))
			self->keyframe_ns_ = pts;
	}
	if (timing)
		self->clock_.observe(timing->cts, packet->pts, packet->timebase_den);
}

void ObsBridge::refresh_metadata(obs_encoder_t *encoder) const
{
	obs_video_info video{};
	const bool have_video = obs_get_video_info(&video);
	metadata_.fps = have_video && video.fps_num && video.fps_den ? VideoFrameRate{video.fps_num, video.fps_den}
								     : VideoFrameRate{60, 1};
	metadata_.width = have_video ? video.output_width : 1920;
	metadata_.height = have_video ? video.output_height : 1080;
	if (encoder) {
		uint32_t divisor = obs_encoder_get_frame_rate_divisor(encoder);
		if (divisor > 1) {
			const auto common = std::gcd(metadata_.fps.num, divisor);
			divisor /= common;
			if (metadata_.fps.den <= std::numeric_limits<uint32_t>::max() / divisor) {
				metadata_.fps.num /= common;
				metadata_.fps.den *= divisor;
			}
		}
		const auto width = obs_encoder_get_width(encoder), height = obs_encoder_get_height(encoder);
		if (width && height) {
			metadata_.width = width;
			metadata_.height = height;
		}
	}
	metadata_encoder_ = encoder;
	// A transient video-info failure must remain retryable on the next poll.
	metadata_valid_ = have_video;
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
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED: {
		std::lock_guard lock(self->clock_mutex_);
		self->clock_.pause(obs_get_video_frame_time());
	}
		emit self->recordingPaused();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED: {
		std::lock_guard lock(self->clock_mutex_);
		self->clock_.resume(obs_get_video_frame_time());
	}
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

namespace {
constexpr const char *binding_names[] = {"hotkey_marker_1", "hotkey_marker_2", "hotkey_marker_3", "hotkey_marker_4",
					 "hotkey_focus_memo"};
}
void ObsBridge::load_hotkey_bindings(obs_data_t *data)
{
	for (int i = 0; i < 5; ++i) {
		auto *bindings = obs_data_get_array(data, binding_names[i]);
		obs_hotkey_load(hotkey_ids_[i], bindings);
		obs_data_array_release(bindings);
	}
}
bool ObsBridge::load_global_hotkeys()
{
	QFile file(QString::fromStdString(hotkeys_path_));
	if (!file.exists())
		return false;
	auto invalid = [this]() {
		hotkeys_writable_ = false;
		emit integrationWarning("Plugin hotkey data could not be read. The file was retained for recovery.");
		return true;
	};
	if (!file.open(QIODevice::ReadOnly))
		return invalid();
	const auto bytes = file.readAll();
	const auto doc = QJsonDocument::fromJson(bytes);
	if (file.error() != QFileDevice::NoError || !doc.isObject())
		return invalid();
	const auto root = doc.object();
	if (root["schema_version"] != QJsonValue(1) || !root["bindings"].isObject())
		return invalid();
	const auto bindings = root["bindings"].toObject();
	for (const auto *key : binding_names)
		if (!bindings[key].isArray())
			return invalid();
	auto *data = obs_data_create_from_json(QJsonDocument(bindings).toJson(QJsonDocument::Compact).constData());
	if (!data)
		return invalid();
	load_hotkey_bindings(data);
	obs_data_release(data);
	return true;
}
bool ObsBridge::save_global_hotkeys()
{
	if (!hotkeys_writable_ || hotkeys_path_.empty())
		return false;
	auto *data = obs_data_create();
	if (!data)
		return false;
	for (int i = 0; i < 5; ++i) {
		auto *bindings = obs_hotkey_save(hotkey_ids_[i]);
		if (!bindings) {
			obs_data_release(data);
			return false;
		}
		obs_data_set_array(data, binding_names[i], bindings);
		obs_data_array_release(bindings);
	}
	const auto bindings = QJsonDocument::fromJson(QByteArray(obs_data_get_json(data))).object();
	obs_data_release(data);
	const auto bytes = QJsonDocument(QJsonObject{{"schema_version", 1}, {"bindings", bindings}}).toJson();
	QFileInfo path(QString::fromStdString(hotkeys_path_));
	if (!path.dir().mkpath("."))
		return false;
	QSaveFile file(path.filePath());
	if (!file.open(QIODevice::WriteOnly))
		return false;
	if (file.write(bytes) != bytes.size()) {
		file.cancelWriting();
		return false;
	}
	return file.commit();
}
void ObsBridge::on_save(obs_data_t *save_data, bool saving, void *private_data)
{
	auto *self = static_cast<ObsBridge *>(private_data);
	if (!self || !self->initialized_ || !save_data)
		return;
	if (saving) {
		if (self->hotkeys_checked_ && self->hotkeys_writable_ && !self->save_global_hotkeys())
			emit self->integrationWarning(
				"Could not save plugin hotkeys. Previous hotkey data was retained.");
	} else if (!self->hotkeys_checked_) {
		// One-time import from the current legacy scene collection. Once loaded,
		// plugin-global bindings take precedence across collection/profile changes.
		self->load_hotkey_bindings(save_data);
		self->hotkeys_checked_ = true;
		if (!self->save_global_hotkeys())
			emit self->integrationWarning(
				"Could not migrate plugin hotkeys. Existing scene data was retained.");
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

std::string ObsBridge::config_path(const char *file) const
{
	char *path = obs_module_config_path(file);
	if (!path)
		return {};
	std::string result = path;
	bfree(path);
	return result;
}
std::string ObsBridge::recovery_cache_directory() const
{
	return config_path("cache");
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
	value.path = get_current_record_file_path();
	// Encoder identity is cheap to check; fixed metadata is refreshed only
	// for a new output/encoder. No output is acquired from this polling path.
	auto *encoder = obs_output_get_video_encoder(recording_output_);
	if (!metadata_valid_ || encoder != metadata_encoder_)
		refresh_metadata(encoder);
	{
		std::lock_guard lock(clock_mutex_);
		clock_fps_ = metadata_.fps;
		value.clock_ready = clock_.ready();
		value.total_frames =
			TimecodeHelper::ns_to_frame_index(clock_.time(obs_get_video_frame_time()), metadata_.fps);
	}
	value.fps = metadata_.fps;
	value.width = metadata_.width;
	value.height = metadata_.height;
	return value;
}
void ObsBridge::drain_pending_events()
{
	QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
}
