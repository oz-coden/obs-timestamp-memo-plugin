#include "plugin-runtime.hpp"
PluginRuntime::PluginRuntime(QMainWindow &main)
	: config(bridge.config_path("settings.ini")),
	  store(bridge.recovery_cache_directory()),
	  controller(bridge, config, store, exporters),
	  notifier(main, config)
{
	QObject::connect(&controller, &SessionController::notificationRequested, &notifier, &StatusNotifier::notify,
			 Qt::AutoConnection);
	notifier.initialize();
	controller.initialize();
}
void PluginRuntime::shutdown()
{
	controller.shutdown();
	notifier.shutdown();
}
