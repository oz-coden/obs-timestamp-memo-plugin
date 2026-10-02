#include "obs-stubs.hpp"
#include "plugin-support.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <vector>
#include <QByteArray>

struct obs_data_t {
	std::string path;
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
char *obs_module_config_path(const char *)
{
	QByteArray path = cache_dir.toUtf8();
	auto *result = static_cast<char *>(std::malloc(static_cast<size_t>(path.size()) + 1));
	std::memcpy(result, path.constData(), static_cast<size_t>(path.size()) + 1);
	return result;
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
void obs_data_release(obs_data_t *) {}
bool obs_get_video_info(obs_video_info *info)
{
	++metadata_query_count;
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
}
obs_data_array_t *obs_hotkey_save(obs_hotkey_id)
{
	return nullptr;
}
void obs_hotkey_load(obs_hotkey_id, obs_data_array_t *) {}
void obs_data_set_array(obs_data_t *, const char *, obs_data_array_t *) {}
obs_data_array_t *obs_data_get_array(obs_data_t *, const char *)
{
	return nullptr;
}
void obs_data_array_release(obs_data_array_t *) {}
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
	invalid_calls = 0;
	active_encoder = &encoder;
	metadata_query_count = 0;
}
void start(const QString &path, int frames)
{
	output_ready = true;
	output.settings.path = path.toStdString();
	output.frames = frames;
	recording = true;
	paused = false;
	dispatch(OBS_FRONTEND_EVENT_RECORDING_STARTED);
}
void stop()
{
	recording = false;
	paused = false;
	dispatch(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
}
void split(const QString &path, int frames)
{
	output.frames = frames;
	calldata_t data{path.toStdString()};
	for (auto callback : output.handler.callbacks)
		callback.first(callback.second, &data);
}
void set_frames(int frames)
{
	output.frames = frames;
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
