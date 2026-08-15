#include "obs-bridge.hpp"
#include "plugin-config.hpp"
#include "status-notifier.hpp"
#include "csv-exporter.hpp"
#include "edl-exporter.hpp"
#include "json-exporter.hpp"
#include "srt-exporter.hpp"
#include "xml-exporter.hpp"

#include <QFileInfo>
#include <QDir>
#include <obs-frontend-api.h>
#include <obs.h>

ObsBridge &ObsBridge::instance()
{
	static ObsBridge inst;
	return inst;
}

ObsBridge::ObsBridge()
{
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

	hotkey_marker_1_id_ = obs_hotkey_register_frontend("obs_timestamp_memo_marker_1",
							   "Timestamp Memo: Quick Marker 1", on_hotkey_marker_1, this);

	hotkey_marker_2_id_ = obs_hotkey_register_frontend("obs_timestamp_memo_marker_2",
							   "Timestamp Memo: Quick Marker 2", on_hotkey_marker_2, this);

	hotkey_marker_3_id_ = obs_hotkey_register_frontend("obs_timestamp_memo_marker_3",
							   "Timestamp Memo: Quick Marker 3", on_hotkey_marker_3, this);

	hotkey_marker_4_id_ = obs_hotkey_register_frontend("obs_timestamp_memo_marker_4",
							   "Timestamp Memo: Quick Marker 4", on_hotkey_marker_4, this);

	hotkey_focus_memo_id_ = obs_hotkey_register_frontend(
		"obs_timestamp_memo_focus_input", "Timestamp Memo: Focus Memo Input", on_hotkey_focus_memo, this);

	initialized_ = true;
	obs_log(LOG_INFO, "[Timestamp Memo] ObsBridge initialized");
}

void ObsBridge::shutdown()
{
	if (!initialized_)
		return;

	obs_frontend_remove_event_callback(on_frontend_event, this);
	obs_frontend_remove_save_callback(on_save, this);

	if (hotkey_marker_1_id_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_marker_1_id_);
		hotkey_marker_1_id_ = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_marker_2_id_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_marker_2_id_);
		hotkey_marker_2_id_ = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_marker_3_id_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_marker_3_id_);
		hotkey_marker_3_id_ = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_marker_4_id_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_marker_4_id_);
		hotkey_marker_4_id_ = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_focus_memo_id_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_focus_memo_id_);
		hotkey_focus_memo_id_ = OBS_INVALID_HOTKEY_ID;
	}

	initialized_ = false;
}

bool ObsBridge::is_recording() const
{
	return obs_frontend_recording_active();
}

bool ObsBridge::is_paused() const
{
	return obs_frontend_recording_paused();
}

uint64_t ObsBridge::get_current_record_ms() const
{
	if (!obs_frontend_recording_active()) {
		return 0;
	}

	obs_output_t *output = obs_frontend_get_recording_output();
	if (!output) {
		return 0;
	}

	uint64_t total_ms = obs_output_get_total_time(output);
	obs_output_release(output);
	return total_ms;
}

std::string ObsBridge::get_current_record_file_path() const
{
	std::string file_path = "";
	obs_output_t *output = obs_frontend_get_recording_output();
	if (output) {
		obs_data_t *settings = obs_output_get_settings(output);
		if (settings) {
			const char *path = obs_data_get_string(settings, "path");
			if (!path || !*path) {
				path = obs_data_get_string(settings, "url");
			}
			if (path && *path) {
				file_path = path;
			}
			obs_data_release(settings);
		}
		obs_output_release(output);
	}
	return file_path;
}

VideoFrameRate ObsBridge::get_current_frame_rate() const
{
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		return VideoFrameRate{ovi.fps_num, ovi.fps_den};
	}
	return VideoFrameRate{60, 1};
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
}

bool ObsBridge::trigger_marker(int type_index, const std::string &custom_comment)
{
	if (!is_recording()) {
		StatusNotifier::instance().notify("Not recording! Cannot stamp marker.", 2000);
		return false;
	}

	if (type_index < 0 || type_index >= 4) {
		type_index = 0;
	}

	const auto &cfg = PluginConfig::instance();
	std::string label = cfg.marker_types[type_index].label;
	std::string color = cfg.marker_types[type_index].color;
	bool paused = is_paused();
	uint64_t ms = get_current_record_ms();

	MemoMarker m = session_.add_marker(ms, type_index, label, color, custom_comment, paused);

	std::string tc = m.active_timecode(session_.frame_rate());
	std::string msg = "[" + label + "] " + tc + (paused ? " (Paused)" : "") + " Recorded";
	StatusNotifier::instance().notify(msg, 3000);

	emit markerAdded(m);
	return true;
}

bool ObsBridge::add_memo_marker(const std::string &text, int type_index)
{
	return trigger_marker(type_index, text);
}

void ObsBridge::perform_auto_export(const std::string &base_video_path)
{
	if (base_video_path.empty()) {
		return;
	}

	QFileInfo fi(QString::fromStdString(base_video_path));
	QDir dir = fi.dir();
	QString base_name = fi.completeBaseName();

	const auto &auto_cfg = PluginConfig::instance().auto_export;

	if (auto_cfg.json) {
		JsonExporter exp;
		std::string path = dir.filePath(base_name + ".json").toStdString();
		exp.export_to_file(session_, path);
	}
	if (auto_cfg.csv) {
		CsvExporter exp;
		std::string path = dir.filePath(base_name + ".csv").toStdString();
		exp.export_to_file(session_, path);
	}
	if (auto_cfg.edl) {
		EdlExporter exp;
		std::string path = dir.filePath(base_name + ".edl").toStdString();
		exp.export_to_file(session_, path);
	}
	if (auto_cfg.srt) {
		SrtExporter exp;
		std::string path = dir.filePath(base_name + ".srt").toStdString();
		exp.export_to_file(session_, path);
	}
	if (auto_cfg.xml) {
		XmlExporter exp;
		std::string path = dir.filePath(base_name + ".xml").toStdString();
		exp.export_to_file(session_, path);
	}
}

void ObsBridge::handle_recording_started()
{
	std::string path = get_current_record_file_path();

	VideoFrameRate fps = get_current_frame_rate();
	uint32_t width = 1920, height = 1080;
	get_video_dimension(width, height);

	session_.start_session(path, fps, width, height);

	emit markersCleared();
	emit recordingStarted(QString::fromStdString(path));
	StatusNotifier::instance().notify("Recording started", 2000);
}

void ObsBridge::handle_recording_paused()
{
	emit recordingPaused();
	StatusNotifier::instance().notify("Recording paused", 2000);
}

void ObsBridge::handle_recording_unpaused()
{
	emit recordingUnpaused();
	StatusNotifier::instance().notify("Recording resumed", 2000);
}

void ObsBridge::handle_recording_stopped()
{
	std::string video_path = session_.video_path();
	if (video_path.empty()) {
		video_path = get_current_record_file_path();
	}

	std::string json_path;
	session_.stop_session(&json_path);

	perform_auto_export(video_path);

	emit recordingStopped(QString::fromStdString(json_path));
	StatusNotifier::instance().notify("Recording stopped. Markers saved.", 3000);
}

void ObsBridge::handle_recording_file_changed()
{
	std::string old_video_path = session_.video_path();
	std::string old_json_path;
	session_.stop_session(&old_json_path);
	perform_auto_export(old_video_path);

	std::string new_path = get_current_record_file_path();

	VideoFrameRate fps = get_current_frame_rate();
	uint32_t width = 1920, height = 1080;
	get_video_dimension(width, height);

	session_.start_session(new_path, fps, width, height);

	emit markersCleared();
	emit recordingFileChanged(QString::fromStdString(new_path));
	StatusNotifier::instance().notify("Recording split into: " + new_path, 3000);
}

void ObsBridge::on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	auto *self = static_cast<ObsBridge *>(private_data);
	if (!self)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		self->handle_recording_started();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		self->handle_recording_paused();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		self->handle_recording_unpaused();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		self->handle_recording_stopped();
		break;
#if defined(OBS_FRONTEND_EVENT_RECORDING_FILE_CHANGED)
	case OBS_FRONTEND_EVENT_RECORDING_FILE_CHANGED:
		self->handle_recording_file_changed();
		break;
#endif
	default:
		break;
	}
}

void ObsBridge::on_save(obs_data_t *save_data, bool saving, void *private_data)
{
	auto *self = static_cast<ObsBridge *>(private_data);
	if (!self)
		return;

	if (saving) {
		obs_data_array_t *hotkey_arr_1 = obs_hotkey_save(self->hotkey_marker_1_id_);
		obs_data_array_t *hotkey_arr_2 = obs_hotkey_save(self->hotkey_marker_2_id_);
		obs_data_array_t *hotkey_arr_3 = obs_hotkey_save(self->hotkey_marker_3_id_);
		obs_data_array_t *hotkey_arr_4 = obs_hotkey_save(self->hotkey_marker_4_id_);
		obs_data_array_t *hotkey_arr_focus = obs_hotkey_save(self->hotkey_focus_memo_id_);

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

		obs_hotkey_load(self->hotkey_marker_1_id_, hotkey_arr_1);
		obs_hotkey_load(self->hotkey_marker_2_id_, hotkey_arr_2);
		obs_hotkey_load(self->hotkey_marker_3_id_, hotkey_arr_3);
		obs_hotkey_load(self->hotkey_marker_4_id_, hotkey_arr_4);
		obs_hotkey_load(self->hotkey_focus_memo_id_, hotkey_arr_focus);

		obs_data_array_release(hotkey_arr_1);
		obs_data_array_release(hotkey_arr_2);
		obs_data_array_release(hotkey_arr_3);
		obs_data_array_release(hotkey_arr_4);
		obs_data_array_release(hotkey_arr_focus);
	}
}

void ObsBridge::on_hotkey_marker_1(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *self = static_cast<ObsBridge *>(data);
	if (self) {
		self->trigger_marker(0);
	}
}

void ObsBridge::on_hotkey_marker_2(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *self = static_cast<ObsBridge *>(data);
	if (self) {
		self->trigger_marker(1);
	}
}

void ObsBridge::on_hotkey_marker_3(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *self = static_cast<ObsBridge *>(data);
	if (self) {
		self->trigger_marker(2);
	}
}

void ObsBridge::on_hotkey_marker_4(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *self = static_cast<ObsBridge *>(data);
	if (self) {
		self->trigger_marker(3);
	}
}

void ObsBridge::on_hotkey_focus_memo(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *self = static_cast<ObsBridge *>(data);
	if (self) {
		emit self->focusMemoInputRequested();
	}
}
