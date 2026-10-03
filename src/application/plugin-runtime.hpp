#pragma once
#include "obs-bridge.hpp"
#include "plugin-config.hpp"
#include "exporter-registry.hpp"
#include "session-store.hpp"
#include "session-controller.hpp"
#include "status-notifier.hpp"
#include "obs-translator.hpp"
class QMainWindow;
class PluginRuntime {
private:
	ObsTranslator translator_; // Installed before services/views; removed after their destruction.
public:
	explicit PluginRuntime(QMainWindow &main);
	void shutdown();
	ObsBridge bridge;
	PluginConfig config;
	ExporterRegistry exporters;
	SessionStore store;
	SessionController controller;
	StatusNotifier notifier;
};
