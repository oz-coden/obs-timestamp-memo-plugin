#include "obs-stubs.hpp"
#include "plugin-support.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <vector>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

struct obs_data_t {
	std::string path;
	QJsonObject values;
	QByteArray json;
	bool allocated = false;
};
struct obs_data_array_t {
	QJsonArray values;
};
struct obs_encoder_t {
	uint32_t divisor = 1, width = 1920, height = 1080;
};
struct calldata_t {
	std::string path;
};
struct signal_handler_t {
	std::vector<std::pair<signal_callback, void *>> callbacks;
};
struct obs_output_t {
	int frames = 0;
	obs_data_t settings;
	signal_handler_t handler;
};

namespace {
obs_output_t output;
obs_encoder_t encoder;
obs_encoder_t replacement_encoder;
obs_encoder_t *active_encoder = &encoder;
int metadata_query_count = 0;
uint64_t video_clock = 1000000000ULL;
bool timed = true;
std::vector<std::pair<packet_callback, void *>> packet_callbacks;
bool have_video_info = true;
bool recording = false, paused = false, exited = false, reject = false;
bool output_ready = false;
int output_queries = 0;
int refs = 0, invalid_calls = 0;
QMainWindow *main_window = nullptr;
QDockWidget *outer_dock = nullptr;
QString cache_dir;
obs_video_info video{60000, 1001, 1920, 1080};
std::vector<std::pair<obs_frontend_event_cb, void *>> event_callbacks;
std::vector<std::pair<obs_frontend_save_cb, void *>> save_callbacks;
std::map<obs_hotkey_id, std::pair<hotkey_callback, void *>> hotkey_callbacks;
obs_hotkey_id next_hotkey = 1;
std::map<obs_hotkey_id, QJsonArray> binding_values;
void frontend_call()
{
	if (exited)
		++invalid_calls;
}
void dispatch(obs_frontend_event event)
{
	for (size_t i = event_callbacks.size(); i > 0; --i) {
		auto callback = event_callbacks.at(i - 1);
		callback.first(event, callback.second);
	}
}
} // namespace

extern "C" {
const char *PLUGIN_NAME = "obs-timestamp-memo";
const char *PLUGIN_VERSION = "test";
void obs_log(int, const char *, ...) {}
}
char *obs_module_config_path(const char *file)
{
	QByteArray path = (std::strcmp(file, "cache") == 0 ? cache_dir : cache_dir + "/" + file).toUtf8();
	auto *result = static_cast<char *>(std::malloc(static_cast<size_t>(path.size()) + 1));
	std::memcpy(result, path.constData(), static_cast<size_t>(path.size()) + 1);
	return result;
}
uint64_t obs_get_video_frame_time()
{
	frontend_call();
	return video_clock;
}
uint32_t obs_output_get_flags(obs_output_t *)
{
	return OBS_OUTPUT_ENCODED | (timed ? OBS_OUTPUT_AUDIO : 0);
}
void obs_output_add_packet_callback(obs_output_t *, packet_callback cb, void *data)
{
	::packet_callbacks.emplace_back(cb, data);
}
void obs_output_remove_packet_callback(obs_output_t *, packet_callback cb, void *data)
{
	std::erase(::packet_callbacks, std::make_pair(cb, data));
}
int obs_output_get_total_frames(obs_output_t *ptr)
{
	return ptr ? ptr->frames : 0;
}
obs_encoder_t *obs_output_get_video_encoder(obs_output_t *)
{
	return active_encoder;
}
uint32_t obs_encoder_get_frame_rate_divisor(obs_encoder_t *ptr)
{
	return ptr->divisor;
}
uint32_t obs_encoder_get_width(obs_encoder_t *ptr)
{
	return ptr->width;
}
uint32_t obs_encoder_get_height(obs_encoder_t *ptr)
{
	return ptr->height;
}
void obs_output_release(obs_output_t *)
{
	--refs;
}
obs_data_t *obs_output_get_settings(obs_output_t *ptr)
{
	return &ptr->settings;
}
const char *obs_data_get_string(obs_data_t *ptr, const char *key)
{
	return std::strcmp(key, "path") == 0 ? ptr->path.c_str() : "";
}
void obs_data_release(obs_data_t *data)
{
	if (data && data->allocated)
		delete data;
}
obs_data_t *obs_data_create()
{
	auto *data = new obs_data_t;
	data->allocated = true;
	return data;
}
obs_data_t *obs_data_create_from_json(const char *json)
{
	auto *data = obs_data_create();
	data->values = QJsonDocument::fromJson(json).object();
	return data;
}
const char *obs_data_get_json(obs_data_t *data)
{
	data->json = QJsonDocument(data->values).toJson();
	return data->json.constData();
}
bool obs_get_video_info(obs_video_info *info)
{
	++metadata_query_count;
	if (!have_video_info)
		return false;
	*info = video;
	return true;
}
obs_hotkey_id obs_hotkey_register_frontend(const char *, const char *, hotkey_callback callback, void *data)
{
	auto id = next_hotkey++;
	hotkey_callbacks[id] = {callback, data};
	return id;
}
void obs_hotkey_unregister(obs_hotkey_id id)
{
	hotkey_callbacks.erase(id);
	binding_values.erase(id);
}
obs_data_array_t *obs_hotkey_save(obs_hotkey_id id)
{
	return new obs_data_array_t{binding_values[id]};
}
void obs_hotkey_load(obs_hotkey_id id, obs_data_array_t *data)
{
	binding_values[id] = data ? data->values : QJsonArray{};
}
void obs_data_set_array(obs_data_t *data, const char *key, obs_data_array_t *array)
{
	data->values[key] = array ? array->values : QJsonArray{};
}
obs_data_array_t *obs_data_get_array(obs_data_t *data, const char *key)
{
	return new obs_data_array_t{data->values[key].toArray()};
}
void obs_data_array_release(obs_data_array_t *data)
{
	delete data;
}
signal_handler_t *obs_output_get_signal_handler(obs_output_t *ptr)
{
	return &ptr->handler;
}
void signal_handler_connect(signal_handler_t *handler, const char *, signal_callback callback, void *data)
{
	handler->callbacks.emplace_back(callback, data);
}
void signal_handler_disconnect(signal_handler_t *handler, const char *, signal_callback callback, void *data)
{
	std::erase(handler->callbacks, std::make_pair(callback, data));
}
const char *calldata_string(calldata_t *data, const char *)
{
	return data->path.c_str();
}
void obs_frontend_add_event_callback(obs_frontend_event_cb cb, void *data)
{
	frontend_call();
	event_callbacks.emplace_back(cb, data);
}
void obs_frontend_remove_event_callback(obs_frontend_event_cb cb, void *data)
{
	frontend_call();
	std::erase(event_callbacks, std::make_pair(cb, data));
}
void obs_frontend_add_save_callback(obs_frontend_save_cb cb, void *data)
{
	frontend_call();
	save_callbacks.emplace_back(cb, data);
}
void obs_frontend_remove_save_callback(obs_frontend_save_cb cb, void *data)
{
	frontend_call();
	std::erase(save_callbacks, std::make_pair(cb, data));
}
bool obs_frontend_recording_active()
{
	frontend_call();
	return recording;
}
bool obs_frontend_recording_paused()
{
	frontend_call();
	return paused;
}
obs_output_t *obs_frontend_get_recording_output()
{
	frontend_call();
	++output_queries;
	// OBS dereferences outputHandler before returning an output. During module
	// loading that handler does not exist, so even a caller's null check is too late.
	if (!output_ready) {
		++invalid_calls;
		return nullptr;
	}
	++refs;
	return &output;
}
void *obs_frontend_get_main_window()
{
	frontend_call();
	return main_window;
}
bool obs_frontend_add_dock_by_id(const char *, const char *title, void *widget)
{
	frontend_call();
	if (reject)
		return false;
	outer_dock = new QDockWidget(QString::fromUtf8(title), main_window);
	outer_dock->setWidget(static_cast<QWidget *>(widget));
	main_window->addDockWidget(Qt::RightDockWidgetArea, outer_dock);
	return true;
}
void obs_frontend_remove_dock(const char *)
{
	frontend_call();
	// Exercise a host that defers outer dock destruction.
	outer_dock->deleteLater();
	outer_dock = nullptr;
}

namespace FakeObs {
void reset(QMainWindow *window, const QString &dir)
{
	main_window = window;
	cache_dir = dir;
	recording = paused = exited = reject = false;
	output_ready = false;
	output_queries = 0;
	video_clock = 1000000000ULL;
	timed = true;
	invalid_calls = 0;
	active_encoder = &encoder;
	metadata_query_count = 0;
	have_video_info = true;
}
void start(const QString &path, int frames)
{
	output_ready = true;
	output.settings.path = path.toStdString();
	output.frames = frames;
	recording = true;
	paused = false;
	dispatch(OBS_FRONTEND_EVENT_RECORDING_STARTED);
	set_frames(frames);
}
void stop()
{
	recording = false;
	paused = false;
	dispatch(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
}
void split(const QString &path, int frames)
{
	set_frames(frames);
	video_packet(static_cast<int64_t>(frames) * video.fps_den, static_cast<int32_t>(video.fps_num), video_clock,
		     true);
	calldata_t data{path.toStdString()};
	for (auto callback : output.handler.callbacks)
		callback.first(callback.second, &data);
}
void set_frames(int frames)
{
	output.frames = frames;
	video_clock = 1000000000ULL + static_cast<uint64_t>(frames) * 1000000000ULL * video.fps_den *
					      active_encoder->divisor / video.fps_num;
	video_packet(static_cast<int64_t>(frames) * video.fps_den * active_encoder->divisor,
		     static_cast<int32_t>(video.fps_num), video_clock);
}
void set_video_clock(uint64_t ns)
{
	video_clock = ns;
}
void timed_packets(bool enabled)
{
	timed = enabled;
}
int packet_callbacks()
{
	return static_cast<int>(::packet_callbacks.size());
}
void video_packet(int64_t pts, int32_t denominator, uint64_t cts, bool keyframe)
{
	encoder_packet packet;
	packet.pts = pts;
	packet.timebase_den = denominator;
	packet.keyframe = keyframe;
	encoder_packet_time timing{cts};
	for (const auto &callback : ::packet_callbacks)
		callback.first(&output, &packet, &timing, callback.second);
}
void set_paused(bool value)
{
	paused = value;
	dispatch(value ? OBS_FRONTEND_EVENT_RECORDING_PAUSED : OBS_FRONTEND_EVENT_RECORDING_UNPAUSED);
}
void set_rate(uint32_t num, uint32_t den)
{
	video.fps_num = num;
	video.fps_den = den;
}
void set_encoder(uint32_t divisor, uint32_t width, uint32_t height)
{
	*active_encoder = {divisor, width, height};
}
void replace_encoder(uint32_t divisor, uint32_t width, uint32_t height)
{
	replacement_encoder = {divisor, width, height};
	active_encoder = &replacement_encoder;
}
int metadata_queries()
{
	return metadata_query_count;
}
void video_info_available(bool available)
{
	have_video_info = available;
}
void set_hotkey_binding(int index, const QString &key)
{
	auto it = hotkey_callbacks.begin();
	std::advance(it, index);
	if (it != hotkey_callbacks.end())
		binding_values[it->first] = QJsonArray{QJsonObject{{"key", key}}};
}
QString hotkey_binding(int index)
{
	auto it = hotkey_callbacks.begin();
	std::advance(it, index);
	if (it == hotkey_callbacks.end() || binding_values[it->first].isEmpty())
		return {};
	return binding_values[it->first].first().toObject()["key"].toString();
}
void load_scene_hotkeys(const QString &key)
{
	obs_data_t data;
	data.values["hotkey_marker_1"] = QJsonArray{QJsonObject{{"key", key}}};
	for (const auto &callback : ::save_callbacks)
		callback.first(&data, false, callback.second);
}
void save_scene_hotkeys()
{
	obs_data_t data;
	for (const auto &callback : ::save_callbacks)
		callback.first(&data, true, callback.second);
}
void hotkey(int index)
{
	auto it = hotkey_callbacks.begin();
	std::advance(it, index);
	if (it != hotkey_callbacks.end())
		it->second.first(it->second.second, it->first, nullptr, true);
}
void exit()
{
	dispatch(OBS_FRONTEND_EVENT_EXIT);
	exited = true;
}
void reject_dock(bool value)
{
	reject = value;
}
int callbacks()
{
	return static_cast<int>(event_callbacks.size());
}
int save_callbacks()
{
	return static_cast<int>(::save_callbacks.size());
}
int output_refs()
{
	return refs;
}
int recording_output_queries()
{
	return output_queries;
}
int file_callbacks()
{
	return static_cast<int>(output.handler.callbacks.size());
}
int hotkeys()
{
	return static_cast<int>(hotkey_callbacks.size());
}
QDockWidget *dock()
{
	return outer_dock;
}
int invalid_frontend_calls()
{
	return invalid_calls;
}
} // namespace FakeObs
