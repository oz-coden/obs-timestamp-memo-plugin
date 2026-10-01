#pragma once
#include "obs.h"
enum obs_frontend_event {
	OBS_FRONTEND_EVENT_RECORDING_STARTED,
	OBS_FRONTEND_EVENT_RECORDING_PAUSED,
	OBS_FRONTEND_EVENT_RECORDING_UNPAUSED,
	OBS_FRONTEND_EVENT_RECORDING_STOPPED,
	OBS_FRONTEND_EVENT_EXIT
};
using obs_frontend_event_cb = void (*)(obs_frontend_event, void *);
using obs_frontend_save_cb = void (*)(obs_data_t *, bool, void *);
void obs_frontend_add_event_callback(obs_frontend_event_cb, void *);
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *);
void obs_frontend_add_save_callback(obs_frontend_save_cb, void *);
void obs_frontend_remove_save_callback(obs_frontend_save_cb, void *);
bool obs_frontend_recording_active();
bool obs_frontend_recording_paused();
obs_output_t *obs_frontend_get_recording_output();
void *obs_frontend_get_main_window();
bool obs_frontend_add_dock_by_id(const char *, const char *, void *);
void obs_frontend_remove_dock(const char *);
