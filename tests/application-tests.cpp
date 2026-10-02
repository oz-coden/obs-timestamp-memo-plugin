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
#include <QUuid>
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
static void unsaved_protection(SessionController &controller, FakeGateway &gateway, const QString &dir)
{
	QFile blocked(dir + "/blocked");
	CHECK(blocked.open(QIODevice::WriteOnly));
	blocked.close();
	gateway.value.path = (blocked.fileName() + "/old.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "must survive split"));
	gateway.split((dir + "/new.mkv").toStdString(), 60);
	CHECK(controller.is_recording() && controller.session().get_markers().empty());
	CHECK(controller.unsaved_documents().size() == 1);
	const auto id = controller.unsaved_documents().front().id;
	RecordingSession rescued;
	CHECK(controller.read_unsaved_document(id, rescued));
	CHECK(rescued.get_markers().front().comment == "must survive split");
	CHECK(!controller.save_unsaved_document(id, (blocked.fileName() + "/save.json").toStdString()));
	CHECK(controller.unsaved_documents().size() == 1);
	CHECK(controller.save_unsaved_document(id, (dir + "/rescued.json").toStdString()));
	CHECK(controller.unsaved_documents().empty());
	gateway.stop();
	CHECK(!controller.has_unsaved_current());
	// Journal failure with a subsequently writable JSON destination is safe.
	gateway.value.path = (dir + "/next-recording.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0));
	gateway.stop();
	CHECK(!controller.has_unsaved_current());
	gateway.value.path = (blocked.fileName() + "/stop.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "must survive stop and restart"));
	gateway.stop();
	CHECK(controller.has_unsaved_current());
	gateway.value.path = (dir + "/after-stop.mkv").toStdString();
	gateway.start();
	CHECK(controller.unsaved_documents().size() == 1);
	CHECK(controller.read_unsaved_document(controller.unsaved_documents().front().id, rescued));
	CHECK(rescued.get_markers().front().comment == "must survive stop and restart");
	gateway.stop();
	// A valid journal can back an unsaved entry without holding a snapshot.
	CHECK(QDir().mkpath(dir + "/journal-only.json"));
	gateway.value.path = (dir + "/journal-only.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "journal survives"));
	gateway.stop();
	CHECK(controller.has_unsaved_current());
	gateway.value.path = (dir + "/after-journal.mkv").toStdString();
	gateway.start();
	CHECK(!controller.unsaved_documents().back().memory);
	CHECK(controller.read_unsaved_document(controller.unsaved_documents().back().id, rescued));
	CHECK(rescued.get_markers().front().comment == "journal survives");
	gateway.stop();
	CHECK(QDir().mkpath(dir + "/edited-journal.json"));
	gateway.value.path = (dir + "/edited-journal.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "before stop"));
	gateway.stop();
	CHECK(controller.update_marker_data(controller.session().get_markers().front().id, "edited", "", "after stop"));
	gateway.value.path = (dir + "/after-edit.mkv").toStdString();
	gateway.start();
	CHECK(controller.read_unsaved_document(controller.unsaved_documents().back().id, rescued));
	CHECK(rescued.get_markers().front().comment == "after stop");
	gateway.stop();
}
static void bounded_unsaved_protection(const QString &dir)
{
	PluginConfig config(QSettings::IniFormat);
	SessionStore store;
	ExporterRegistry exporters;
	FakeGateway gateway;
	SessionController controller(gateway, config, store, exporters);
	controller.initialize();
	QFile blocked(dir + "/bounded-blocked");
	CHECK(blocked.open(QIODevice::WriteOnly));
	blocked.close();
	gateway.value.path = (blocked.fileName() + "/0.mkv").toStdString();
	gateway.start();
	for (int i = 0; i < 9; ++i) {
		CHECK(controller.trigger_quick_marker(0, "segment-" + std::to_string(i)));
		gateway.split((blocked.fileName() + QString("/%1.mkv").arg(i + 1)).toStdString(), 60ULL * (i + 1));
	}
	CHECK(controller.is_recording() && !controller.can_stamp());
	CHECK(controller.unsaved_documents().size() == 8);
	CHECK(controller.session().get_markers().front().comment == "segment-8");
	CHECK(!controller.trigger_quick_marker(0));
	CHECK(controller.save_unsaved_document(controller.unsaved_documents().front().id,
					       (dir + "/bounded-rescue.json").toStdString()));
	CHECK(controller.can_stamp() && controller.unsaved_documents().size() == 8);
	CHECK(controller.trigger_quick_marker(0, "resumed"));
	gateway.stop();
	CHECK(controller.save_current_document((dir + "/current-rescue.json").toStdString()));
	CHECK(!controller.has_unsaved_current());
}
static void journal_failure_json_success(const QString &dir)
{
	PluginConfig config(QSettings::IniFormat);
	SessionStore store;
	ExporterRegistry exporters;
	FakeGateway gateway;
	SessionController controller(gateway, config, store, exporters);
	controller.initialize();
	QFile blocked(dir + "/temporarily-blocked");
	CHECK(blocked.open(QIODevice::WriteOnly));
	blocked.close();
	gateway.value.path = (blocked.fileName() + "/video.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0));
	CHECK(!store.journal_healthy());
	CHECK(blocked.remove());
	CHECK(QDir().mkpath(blocked.fileName()));
	gateway.stop();
	CHECK(!controller.has_unsaved_current());
	CHECK(QFile::exists(blocked.fileName() + "/video.json"));
	gateway.value.path = (blocked.fileName() + "/next.mkv").toStdString();
	gateway.start();
	CHECK(controller.unsaved_documents().empty());
	gateway.stop();
}
static void durable_unsaved_recovery(const QString &dir)
{
	const auto cache_dir = (dir + "/durable-cache").toStdString();
	const auto blocked_path = dir + "/durable-blocked";
	QFile blocked(blocked_path);
	CHECK(blocked.open(QIODevice::WriteOnly));
	blocked.close();
	{
		PluginConfig config(QSettings::IniFormat);
		SessionStore store(cache_dir);
		ExporterRegistry exporters;
		FakeGateway gateway;
		SessionController controller(gateway, config, store, exporters);
		controller.initialize();
		gateway.value.path = (blocked_path + "/old.mkv").toStdString();
		gateway.start();
		CHECK(controller.trigger_quick_marker(0, "survives runtime destruction"));
		gateway.split((dir + "/durable-new.mkv").toStdString(), 60);
		CHECK(controller.unsaved_documents().size() == 1);
		CHECK(!controller.unsaved_documents().front().memory);
		gateway.stop();
	}
	PluginConfig config(QSettings::IniFormat);
	SessionStore store(cache_dir);
	ExporterRegistry exporters;
	FakeGateway gateway;
	SessionController controller(gateway, config, store, exporters);
	controller.initialize();
	CHECK(controller.unsaved_documents().size() == 1);
	RecordingSession restored;
	const auto id = controller.unsaved_documents().front().id;
	CHECK(controller.read_unsaved_document(id, restored));
	CHECK(restored.get_markers().front().comment == "survives runtime destruction");
	CHECK(controller.save_unsaved_document(id, (dir + "/durable-saved.json").toStdString()));
	CHECK(store.recovery_copies().empty());
}
static void recovery_collision_protection(const QString &dir)
{
	for (const auto &kind : {"same", "different", "corrupt", "missing", "unresolved"}) {
		const auto folder = dir + "/collision-" + kind;
		CHECK(QDir().mkpath(folder));
		RecordingSession original;
		original.replace({kind == std::string("unresolved") ? "" : (folder + "/video.mkv").toStdString(),
				  QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
				  "2026-10-02T00:00:00.000Z",
				  {60, 1},
				  1920,
				  1080});
		const auto marker = original.add_marker_at_frame(60, 0, "label", "#ffffff", "cached", false, "");
		SessionStore journal(folder.toStdString());
		CHECK(journal.begin(original));
		CHECK(journal.append({{"op", "add"}, {"marker", SessionCodec::encode_marker(marker)}}));
		CHECK(journal.finish(original, false));
		const auto cache = journal.cache_path();
		const auto target = folder + "/video.json";
		if (kind == std::string("same") || kind == std::string("different")) {
			auto existing = original;
			if (kind == std::string("different")) {
				auto meta = SessionMetadata{
					original.video_path(),
					QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
					original.started_at(),
					original.frame_rate(),
					original.width(),
					original.height()};
				existing.replace(meta, original.get_markers());
			}
			SessionStore writer;
			CHECK(writer.save(existing, target.toStdString()));
		} else if (kind == std::string("corrupt")) {
			QFile bad(target);
			CHECK(bad.open(QIODevice::WriteOnly));
			CHECK(bad.write("{broken") == 7);
		}
		QFile before(target);
		const auto bytes = before.open(QIODevice::ReadOnly) ? before.readAll() : QByteArray();
		before.close();
		PluginConfig config(QSettings::IniFormat);
		SessionStore store;
		ExporterRegistry exporters;
		FakeGateway gateway;
		SessionController controller(gateway, config, store, exporters);
		controller.initialize();
		CHECK(controller.recover_from_cache(cache));
		CHECK(controller.update_marker_data(marker.id, "label", "", "edited"));
		const bool safe = kind == std::string("same") || kind == std::string("missing");
		CHECK(controller.has_unsaved_current() == !safe);
		CHECK(QFile::exists(QString::fromStdString(cache)) == !safe);
		if (!safe && kind != std::string("unresolved")) {
			QFile after(target);
			CHECK(after.open(QIODevice::ReadOnly) && after.readAll() == bytes);
		}
		if (!safe) {
			// Use a file as a directory to exercise a guaranteed write failure.
			QFile blocked(folder + "/blocked");
			CHECK(blocked.open(QIODevice::WriteOnly));
			blocked.close();
			CHECK(!controller.save_current_document((blocked.fileName() + "/save.json").toStdString()));
			CHECK(QFile::exists(QString::fromStdString(cache)));
			CHECK(controller.save_current_document((folder + "/rescued.json").toStdString()));
			CHECK(!QFile::exists(QString::fromStdString(cache)));
		}
	}
}
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
		controller.initialize();
		unsaved_protection(controller, gateway, dir.path());
		controller.shutdown();
		bounded_unsaved_protection(dir.path());
		journal_failure_json_success(dir.path());
		durable_unsaved_recovery(dir.path());
		recovery_collision_protection(dir.path());
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
