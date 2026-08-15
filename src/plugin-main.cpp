#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include "obs-bridge.hpp"
#include "dock-widget.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static DockWidget *g_dock_widget = nullptr;

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "[%s] version %s loading...", PLUGIN_NAME, PLUGIN_VERSION);

	// OBSブリッジ初期化
	ObsBridge::instance().initialize();

	// Qtドックウィジェットの登録
	QMainWindow *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	g_dock_widget = new DockWidget(main_win);
	obs_frontend_add_dock(g_dock_widget);

	obs_log(LOG_INFO, "[%s] loaded successfully", PLUGIN_NAME);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "[%s] unloading...", PLUGIN_NAME);
	ObsBridge::instance().shutdown();
	obs_log(LOG_INFO, "[%s] unloaded", PLUGIN_NAME);
}
