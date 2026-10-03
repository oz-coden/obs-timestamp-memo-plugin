#pragma once
#include <QString>
#include <QMainWindow>
#include <QDockWidget>
#include "obs-frontend-api.h"
namespace FakeObs {
void reset(QMainWindow *window, const QString &cache_dir);
void start(const QString &path, int frames = 0);
void stop();
void split(const QString &path, int frames);
void set_frames(int frames);
void set_paused(bool paused);
void set_rate(uint32_t num, uint32_t den);
void set_encoder(uint32_t divisor, uint32_t width, uint32_t height);
void replace_encoder(uint32_t divisor, uint32_t width, uint32_t height);
int metadata_queries();
void video_info_available(bool available);
void hotkey(int index);
void set_hotkey_binding(int index, const QString &key);
QString hotkey_binding(int index);
void load_scene_hotkeys(const QString &key);
void save_scene_hotkeys();
void exit();
void reject_dock(bool reject);
int callbacks();
int save_callbacks();
int output_refs();
int recording_output_queries();
int file_callbacks();
int hotkeys();
QDockWidget *dock();
int invalid_frontend_calls();
} // namespace FakeObs
