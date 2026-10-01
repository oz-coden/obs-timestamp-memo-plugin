#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QMainWindow>
#include <QPointer>
#include <QThread>

#include "dock-widget.hpp"
#include "session-controller.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static QPointer<DockWidget> g_dock_widget;
static bool g_registered = false;
static bool g_initialized = false;

static void cleanup()
{
	if (!g_initialized)
		return;
	g_initialized = false;
	SessionController::instance().shutdown();
	if (g_registered) {
		g_registered = false;
		obs_frontend_remove_dock("obs_timestamp_memo_dock");
	}
	g_dock_widget.clear();
}

static void on_frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		cleanup();
		obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	}
}

bool obs_module_load(void)
{
	if (g_initialized)
		return true;
	obs_log(LOG_INFO, "[%s] version %s loading...", PLUGIN_NAME, PLUGIN_VERSION);

	auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_win) {
		obs_log(LOG_ERROR, "[Timestamp Memo] OBS main window is unavailable");
		return false;
	}
	if (main_win->thread() != QThread::currentThread()) {
		obs_log(LOG_ERROR, "[Timestamp Memo] Plugin must be loaded on the OBS UI thread");
		return false;
	}
	// Register before ObsBridge: OBS dispatches callbacks in reverse order, so
	// the bridge can unregister itself on EXIT before this callback cleans up.
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	g_initialized = true;
	SessionController::instance().initialize();
	g_dock_widget = new DockWidget(main_win);

	if (!obs_frontend_add_dock_by_id("obs_timestamp_memo_dock", "Timestamp Memo & Markers", g_dock_widget.data())) {
		delete g_dock_widget.data();
		cleanup();
		obs_frontend_remove_event_callback(on_frontend_event, nullptr);
		obs_log(LOG_ERROR, "[Timestamp Memo] Failed to register dock");
		return false;
	}
	g_registered = true;

	obs_log(LOG_INFO, "[%s] loaded successfully", PLUGIN_NAME);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "[%s] unloading...", PLUGIN_NAME);
	if (g_initialized) {
		cleanup();
		obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	}
	obs_log(LOG_INFO, "[%s] unloaded", PLUGIN_NAME);
}
