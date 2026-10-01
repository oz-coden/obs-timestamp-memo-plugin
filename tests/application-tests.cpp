#include "recording-gateway.hpp"
#include "session-controller.hpp"
#include "plugin-config.hpp"
#include "session-store.hpp"
#include "session-codec.hpp"
#include "exporter-registry.hpp"
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <iostream>
#include <stdexcept>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
class FakeGateway final : public RecordingGateway {
public:
	RecordingSnapshot value;
	bool initialized = false;
	void initialize() override { initialized = true; }
	void shutdown() override { initialized = false; }
	RecordingSnapshot snapshot() const override { return value; }
	void drain_pending_events() override {}
	void start()
	{
		value.recording = true;
		emit recordingStarted();
	}
	void pause()
	{
		value.paused = true;
		emit recordingPaused();
	}
	void resume()
	{
		value.paused = false;
		emit recordingUnpaused();
	}
	void split(const std::string &path, uint64_t frames)
	{
		value.path = path;
		value.total_frames = frames;
		emit recordingFileChanged(QString::fromStdString(path), frames);
	}
	void stop()
	{
		value.recording = false;
		value.paused = false;
		emit recordingStopped();
	}
	void quick(int type) { emit quickMarkerRequested(type); }
};
int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	QTemporaryDir dir(QDir::currentPath() + "/application-data-XXXXXX");
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path() + "/settings");
	QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, dir.path() + "/settings");
	try {
		CHECK(dir.isValid());
		PluginConfig config(QSettings::IniFormat);
		CHECK(config.load());
		auto settings = config.values();
		settings.marker_types[1].label = "Custom";
		settings.marker_types[1].color = "#abcdef";
		settings.auto_export.json = false;
		CHECK(config.save(settings));
		PluginConfig reloaded(QSettings::IniFormat);
		CHECK(reloaded.load());
		CHECK(reloaded.values().marker_types[1].label == "Custom");
		CHECK(!reloaded.values().auto_export.json);
		{
			QSettings raw(QSettings::IniFormat, QSettings::UserScope, "oz-coden", "obs-timestamp-memo");
			raw.setValue("Markers/marker_2_color", "invalid color");
			raw.remove("Markers/marker_2_label");
			raw.sync();
		}
		CHECK(reloaded.load());
		CHECK(reloaded.values().marker_types[1].color == "#2ecc71");
		CHECK(reloaded.values().marker_types[1].label == "Chapter");
		settings.auto_export.json = true;
		CHECK(config.save(settings));
		SessionStore store((dir.path() + "/cache").toStdString());
		ExporterRegistry exporters;
		FakeGateway gateway;
		gateway.value.fps = {60000, 1001};
		gateway.value.width = 1280;
		gateway.value.height = 720;
		SessionController controller(gateway, config, store, exporters);
		controller.initialize();
		CHECK(gateway.initialized && !controller.is_recording());
		CHECK(!controller.trigger_quick_marker(0));
		gateway.start();
		CHECK(controller.is_recording());
		gateway.value.total_frames = 60;
		gateway.quick(1);
		CHECK(controller.session().get_markers().size() == 1);
		CHECK(controller.session().get_markers().front().frame_index == 60);
		CHECK(controller.session().get_markers().front().label == "Custom");
		gateway.pause();
		gateway.quick(0);
		CHECK(controller.is_paused() && controller.session().get_markers().back().is_paused);
		gateway.resume();
		gateway.value.path = (dir.path() + "/one.mkv").toStdString();
		controller.check_recording_file_changed();
		CHECK(controller.session().video_path() == gateway.value.path);
		CHECK(!controller.load_from_json("anything.json"));
		auto original = controller.session().session_id();
		gateway.split((dir.path() + "/two.mkv").toStdString(), 60);
		CHECK(controller.session().session_id() != original && controller.current_record_ms() == 0);
		gateway.value.total_frames = 120;
		gateway.quick(2);
		CHECK(controller.session().get_markers().front().timestamp_ms == 1001);
		gateway.split((dir.path() + "/out-of-order.mkv").toStdString(), 59);
		CHECK(controller.session().video_path() == (dir.path() + "/two.mkv").toStdString());
		gateway.stop();
		CHECK(!controller.is_recording() && QFile::exists(dir.path() + "/two.json"));
		CHECK(controller.load_from_json((dir.path() + "/one.json").toStdString()));
		CHECK(controller.document_title() == "one.json (Loaded)");
		CHECK(controller.session().get_markers().size() == 2);
		auto json = SessionCodec::encode(controller.session());
		RecordingSession decoded;
		CHECK(SessionCodec::decode(json, decoded));
		CHECK(SessionCodec::encode(decoded) == json);
		controller.shutdown();
		CHECK(!gateway.initialized);
		controller.initialize();
		gateway.value.path = (dir.path() + "/reload.mkv").toStdString();
		gateway.start();
		gateway.quick(0);
		CHECK(controller.session().get_markers().size() == 1);
		gateway.stop();
		controller.shutdown();
		// A failed settings write must not publish a partly edited live value.
		QFile blocked(dir.path() + "/blocked");
		CHECK(blocked.open(QIODevice::WriteOnly));
		blocked.close();
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, blocked.fileName());
		auto candidate = config.values();
		candidate.marker_types[0].label = "Must not apply";
		CHECK(!config.save(candidate));
		CHECK(config.values().marker_types[0].label != "Must not apply");
		std::cout
			<< "PASS application lifecycle, settings conversion/failure, history and serialization without OBS or Widgets\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
