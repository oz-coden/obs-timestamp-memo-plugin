#pragma once
#include <cstdint>
#include <cstdlib>

struct obs_data_t;
struct obs_data_array_t;
struct obs_output_t;
struct obs_encoder_t;
struct obs_hotkey_t;
struct signal_handler_t;
struct calldata_t;
using obs_hotkey_id = uint64_t;
constexpr obs_hotkey_id OBS_INVALID_HOTKEY_ID = UINT64_MAX;
constexpr int LOG_INFO = 200;
constexpr int LOG_ERROR = 100;
constexpr int LOG_WARNING = 300;
struct obs_video_info {
	uint32_t fps_num, fps_den, output_width, output_height;
};
using signal_callback = void (*)(void *, calldata_t *);
using hotkey_callback = void (*)(void *, obs_hotkey_id, obs_hotkey_t *, bool);
int obs_output_get_total_frames(obs_output_t *);
obs_encoder_t *obs_output_get_video_encoder(obs_output_t *);
uint32_t obs_encoder_get_frame_rate_divisor(obs_encoder_t *);
uint32_t obs_encoder_get_width(obs_encoder_t *);
uint32_t obs_encoder_get_height(obs_encoder_t *);
void obs_output_release(obs_output_t *);
obs_data_t *obs_output_get_settings(obs_output_t *);
const char *obs_data_get_string(obs_data_t *, const char *);
void obs_data_release(obs_data_t *);
obs_data_t *obs_data_create();
obs_data_t *obs_data_create_from_json(const char *);
const char *obs_data_get_json(obs_data_t *);
bool obs_get_video_info(obs_video_info *);
obs_hotkey_id obs_hotkey_register_frontend(const char *, const char *, hotkey_callback, void *);
void obs_hotkey_unregister(obs_hotkey_id);
obs_data_array_t *obs_hotkey_save(obs_hotkey_id);
void obs_hotkey_load(obs_hotkey_id, obs_data_array_t *);
void obs_data_set_array(obs_data_t *, const char *, obs_data_array_t *);
obs_data_array_t *obs_data_get_array(obs_data_t *, const char *);
void obs_data_array_release(obs_data_array_t *);
signal_handler_t *obs_output_get_signal_handler(obs_output_t *);
void signal_handler_connect(signal_handler_t *, const char *, signal_callback, void *);
void signal_handler_disconnect(signal_handler_t *, const char *, signal_callback, void *);
const char *calldata_string(calldata_t *, const char *);
inline void bfree(void *ptr)
{
	std::free(ptr);
}

constexpr uint32_t OBS_OUTPUT_AUDIO = 2, OBS_OUTPUT_ENCODED = 4;
enum obs_encoder_type { OBS_ENCODER_VIDEO, OBS_ENCODER_AUDIO };
struct encoder_packet {
	int64_t pts = 0;
	int32_t timebase_den = 60;
	obs_encoder_type type = OBS_ENCODER_VIDEO;
	size_t track_idx = 0;
	bool keyframe = false;
};
struct encoder_packet_time {
	uint64_t cts = 0;
};
using packet_callback = void (*)(obs_output_t *, encoder_packet *, encoder_packet_time *, void *);
uint64_t obs_get_video_frame_time();
uint32_t obs_output_get_flags(obs_output_t *);
void obs_output_add_packet_callback(obs_output_t *, packet_callback, void *);
void obs_output_remove_packet_callback(obs_output_t *, packet_callback, void *);
