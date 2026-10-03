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
#include <QJsonArray>
#include <limits>
#include <iostream>
#include <stdexcept>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
class FakeGateway final : public RecordingGateway {
public:
	RecordingSnapshot value;
	mutable int snapshots = 0;
	bool initialized = false;
	void initialize() override { initialized = true; }
	void shutdown() override { initialized = false; }
	RecordingSnapshot snapshot() const override
	{
		++snapshots;
		return value;
	}
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
	QFile invalid(dir + "/invalid-open.json");
	CHECK(invalid.open(QIODevice::WriteOnly));
	CHECK(invalid.write("{}") == 2);
	invalid.close();
	const auto pending_before = controller.unsaved_documents().size();
	CHECK(!controller.load_from_json(invalid.fileName().toStdString()));
	CHECK(!controller.recover_from_cache(invalid.fileName().toStdString()));
	CHECK(controller.has_unsaved_current());
	CHECK(controller.unsaved_documents().size() == pending_before);
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
static void codec_validation(const QString &dir)
{
	RecordingSession source;
	source.replace({"video.mkv",
			QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
			"2026-10-02T00:00:00.000Z",
			{60000, 1001},
			1920,
			1080});
	for (const auto ms : {863999999ULL, 864000000ULL, 864000001ULL, 1728000000ULL}) {
		source.clear_markers();
		source.add_marker(ms, 0, "label", "#ffffff", "comment", false);
		RecordingSession decoded;
		CHECK(SessionCodec::decode(SessionCodec::encode(source), decoded));
		CHECK(decoded.get_markers().front().timestamp_ms == ms);
	}
	const auto valid = SessionCodec::encode(source);
	RecordingSession decoded;
	CHECK(SessionCodec::decode(valid, decoded));
	auto legacy = valid;
	legacy.remove("schema_version");
	CHECK(SessionCodec::decode(legacy, decoded));
	QString detail;
	for (const auto field : {"label", "comment", "frame_index", "timestamp_ms", "is_paused"}) {
		auto bad = valid;
		auto array = bad["markers"].toArray();
		auto item = array.first().toObject();
		item[field] = QJsonObject();
		array[0] = item;
		bad["markers"] = array;
		const auto prior = SessionCodec::encode(decoded);
		CHECK(!SessionCodec::decode(bad, decoded, &detail));
		CHECK(detail.contains("Marker #1") && detail.contains(field) && detail.contains("expected") &&
		      detail.contains("actual"));
		CHECK(SessionCodec::encode(decoded) == prior);
		const auto file_path = dir + "/detailed-error.json";
		QFile file(file_path);
		CHECK(file.open(QIODevice::WriteOnly));
		file.write(QJsonDocument(bad).toJson());
		file.close();
		SessionStore reader;
		CHECK(!reader.load(file_path.toStdString(), decoded));
		CHECK(reader.last_error() == detail);
	}
	CHECK(SessionCodec::decode(valid, decoded, &detail) && detail.isEmpty());
	const auto before = SessionCodec::encode(decoded);
	auto reject = [&](const QJsonObject &bad) {
		CHECK(!SessionCodec::decode(bad, decoded, &detail));
		CHECK(!detail.isEmpty());
		CHECK(SessionCodec::encode(decoded) == before);
	};
	for (const auto key : {"session_id", "video_file_path", "started_at_utc", "video_info", "markers"}) {
		auto bad = valid;
		bad.remove(key);
		reject(bad);
		bad = valid;
		bad[key] = false;
		reject(bad);
	}
	for (const auto &schema : {QJsonValue("2.0.0"), QJsonValue(1), QJsonValue("")}) {
		auto bad = valid;
		bad["schema_version"] = schema;
		reject(bad);
	}
	auto bad_id = valid;
	bad_id["session_id"] = "not-a-session-id";
	reject(bad_id);
	for (const auto key :
	     {"id", "timestamp_ms", "frame_index", "type_index", "label", "color", "comment", "is_paused"}) {
		for (const auto &value : {QJsonValue(), QJsonValue(QJsonArray{})}) {
			auto bad = valid;
			auto marker = bad["markers"].toArray().first().toObject();
			marker[key] = value;
			bad["markers"] = QJsonArray{marker};
			reject(bad);
		}
	}
	for (const auto key : {"timestamp_ms", "frame_index", "id", "type_index"}) {
		for (const auto &value :
		     {QJsonValue(-1), QJsonValue(0.5), QJsonValue(std::numeric_limits<qint64>::max())}) {
			auto bad = valid;
			auto marker = bad["markers"].toArray().first().toObject();
			marker[key] = value;
			bad["markers"] = QJsonArray{marker};
			reject(bad);
		}
	}
	for (const auto key : {"fps_num", "fps_den", "width", "height"}) {
		for (const auto &value :
		     {QJsonValue(), QJsonValue("60"), QJsonValue(0), QJsonValue(-1), QJsonValue(0.5)}) {
			auto bad = valid;
			auto video = bad["video_info"].toObject();
			video[key] = value;
			bad["video_info"] = video;
			reject(bad);
		}
	}
	auto bad = valid;
	auto marker = bad["markers"].toArray().first().toObject();
	marker["frame_index"] = 0;
	bad["markers"] = QJsonArray{marker};
	reject(bad);
	bad["markers"] = QJsonArray{valid["markers"].toArray().first(), valid["markers"].toArray().first()};
	reject(bad);
	// The exact JSON integer boundary is supported, not a duration limit.
	RecordingSession boundary;
	boundary.replace({"video.mkv", source.session_id(), source.started_at(), {1, 1}, 1920, 1080});
	boundary.add_marker(9007199254740991ULL, 0, "", "", "", false);
	CHECK(SessionCodec::decode(SessionCodec::encode(boundary), decoded));
	CHECK(decoded.get_markers().front().timestamp_ms == 9007199254740991ULL);
	auto too_large = SessionCodec::encode(boundary);
	auto large_marker = too_large["markers"].toArray().first().toObject();
	large_marker["timestamp_ms"] = static_cast<qint64>(9007199254740992LL);
	too_large["markers"] = QJsonArray{large_marker};
	CHECK(!SessionCodec::decode(too_large, decoded));
	// Legacy journals have no schema; partial updates retain omitted fields.
	SessionStore journal;
	source.set_video_path((dir + "/codec.mkv").toStdString());
	CHECK(journal.begin(source));
	const auto item = source.get_markers().front();
	CHECK(journal.append({{"op", "add"}, {"marker", SessionCodec::encode_marker(item)}}));
	CHECK(journal.finish(source, false));
	QFile input(QString::fromStdString(journal.cache_path()));
	CHECK(input.open(QIODevice::ReadOnly));
	const auto bytes = input.readAll();
	input.close();
	const auto first_line = bytes.indexOf('\n');
	auto header = QJsonDocument::fromJson(bytes.left(first_line)).object();
	header.remove("schema_version");
	auto legacy_bytes = QJsonDocument(header).toJson(QJsonDocument::Compact) + bytes.mid(first_line);
	const auto legacy_path = dir + "/legacy-codec.tmp.jsonl";
	QFile legacy_file(legacy_path);
	CHECK(legacy_file.open(QIODevice::WriteOnly));
	CHECK(legacy_file.write(legacy_bytes) == legacy_bytes.size());
	legacy_file.close();
	SessionStore legacy_reader;
	CHECK(legacy_reader.recover(legacy_path.toStdString(), decoded));
	header["schema_version"] = "2.0.0";
	legacy_bytes = QJsonDocument(header).toJson(QJsonDocument::Compact) + bytes.mid(first_line);
	CHECK(legacy_file.open(QIODevice::WriteOnly));
	CHECK(legacy_file.write(legacy_bytes) == legacy_bytes.size());
	legacy_file.close();
	const auto legacy_before = SessionCodec::encode(decoded);
	CHECK(!legacy_reader.recover(legacy_path.toStdString(), decoded));
	CHECK(legacy_reader.last_error().contains("line 1") && legacy_reader.last_error().contains("schema_version"));
	CHECK(SessionCodec::encode(decoded) == legacy_before);
	auto recover = [&](const QJsonObject &op, bool success) {
		const auto path = dir + "/codec-test.tmp.jsonl";
		QFile output(path);
		CHECK(output.open(QIODevice::WriteOnly));
		const auto data = bytes + QJsonDocument(op).toJson(QJsonDocument::Compact) + "\n";
		CHECK(output.write(data) == data.size());
		output.close();
		SessionStore reader;
		const auto prior = SessionCodec::encode(decoded);
		CHECK(reader.recover(path.toStdString(), decoded) == success);
		if (!success)
			CHECK(SessionCodec::encode(decoded) == prior);
	};
	recover({{"op", "update"}, {"id", static_cast<qint64>(item.id)}, {"comment", "partial"}}, true);
	CHECK(decoded.get_markers().front().label == item.label);
	CHECK(decoded.get_markers().front().color == item.color);
	CHECK(decoded.get_markers().front().comment == "partial");
	recover({{"op", "update"}, {"id", static_cast<qint64>(item.id)}, {"comment", false}}, false);
	recover({{"op", "update"}, {"id", static_cast<qint64>(item.id)}}, false);
	recover({{"op", "delete"}, {"id", "1"}}, false);
	recover({{"op", "video_path"}}, false);
	recover({{"op", "unsupported"}}, false);
	recover({{"op", 1}}, false);
}
static void edit_history_and_recovery(const QString &dir)
{
	PluginConfig config(QSettings::IniFormat);
	auto settings = config.values();
	settings.auto_export.json = false;
	CHECK(config.save(settings));
	SessionStore store((dir + "/undo-cache").toStdString());
	ExporterRegistry exporters;
	FakeGateway gateway;
	SessionController controller(gateway, config, store, exporters);
	controller.initialize();
	gateway.value.path = (dir + "/undo.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "first"));
	const auto first = controller.session().get_markers().front();
	gateway.value.total_frames = 60;
	CHECK(controller.trigger_quick_marker(1, "second"));
	const auto second = controller.session().get_markers().back();
	CHECK(controller.delete_marker(first.id));
	CHECK(controller.undo());
	CHECK(controller.session().get_markers().front().id == first.id);
	CHECK(controller.session().get_markers().back().id == second.id);
	RecordingSession recovered;
	SessionStore reader;
	CHECK(reader.recover(store.cache_path(), recovered));
	CHECK(recovered.get_markers().front().id == first.id);
	CHECK(controller.redo() && controller.undo());
	CHECK(controller.update_marker_data(first.id, "custom", "", "edited"));
	CHECK(controller.update_marker_type(first.id, 3));
	CHECK(controller.undo());
	CHECK(controller.session().get_markers().front().type_index == 0);
	CHECK(controller.undo());
	CHECK(controller.session().get_markers().front().comment == "first");
	CHECK(controller.redo());
	CHECK(controller.session().get_markers().front().label == "custom");
	CHECK(controller.delete_marker(second.id));
	CHECK(!controller.can_redo());
	controller.clear_markers();
	CHECK(controller.undo() && controller.session().get_markers().size() == 1);
	gateway.stop();
	CHECK(!QFile::exists(dir + "/undo.json"));
	const auto journal = store.cache_path();
	CHECK(controller.update_marker_data(first.id, "custom", "", "stopped edit"));
	CHECK(!QFile::exists(dir + "/undo.json") && QFile::exists(QString::fromStdString(journal)));
	CHECK(reader.recover(journal, recovered));
	CHECK(recovered.get_markers().front().comment == "stopped edit");
	CHECK(controller.undo());
	CHECK(reader.recover(journal, recovered));
	CHECK(recovered.get_markers().front().comment == "edited");
	CHECK(controller.redo());
	const auto rescue = (dir + "/undo-explicit.json").toStdString();
	CHECK(controller.save_current_document(rescue));
	CHECK(reader.load(rescue, recovered));
	CHECK(recovered.get_markers().front().comment == "stopped edit");
	settings.auto_export.json = true;
	CHECK(config.save(settings));
	CHECK(controller.undo());
	CHECK(reader.load(rescue, recovered));
	CHECK(recovered.get_markers().front().comment == "edited");
	gateway.value.path = (dir + "/undo-new.mkv").toStdString();
	gateway.start();
	CHECK(!controller.can_undo() && !controller.can_redo());
	CHECK(controller.trigger_quick_marker(0));
	CHECK(controller.undo() && controller.session().get_markers().empty());
	CHECK(controller.redo() && controller.session().get_markers().size() == 1);
	gateway.split((dir + "/undo-split.mkv").toStdString(), 60);
	CHECK(!controller.can_undo());
	gateway.stop();
	CHECK(config.save(PluginSettings{}));
}

static void discard_documents(const QString &dir)
{
	PluginConfig config(QSettings::IniFormat);
	CHECK(config.save(PluginSettings{}));
	SessionStore store((dir + "/discard-cache").toStdString());
	ExporterRegistry exporters;
	FakeGateway gateway;
	SessionController controller(gateway, config, store, exporters);
	controller.initialize();
	CHECK(QDir().mkpath(dir + "/discard.json"));
	gateway.value.path = (dir + "/discard.mkv").toStdString();
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "discard only this"));
	const auto journal = store.cache_path();
	gateway.split((dir + "/discard-next.mkv").toStdString(), 60);
	CHECK(controller.unsaved_documents().size() == 1);
	CHECK(controller.discard_unsaved_document(controller.unsaved_documents().front().id));
	CHECK(controller.unsaved_documents().empty());
	CHECK(!QFile::exists(QString::fromStdString(journal)));
	CHECK(!controller.discard_unsaved_document(9999));
	CHECK(controller.trigger_quick_marker(0, "current remains"));
	CHECK(!controller.discard_unsaved_document(0));
	gateway.stop();
	CHECK(controller.session().get_markers().front().comment == "current remains");
	SessionController restarted(gateway, config, store, exporters);
	controller.shutdown();
	restarted.initialize();
	CHECK(restarted.unsaved_documents().empty());
}

static void safe_exports(const QString &dir)
{
	PluginConfig config(QSettings::IniFormat);
	auto settings = PluginSettings{};
	settings.auto_export = {true, true, true, true, true, true, true, true};
	CHECK(config.save(settings));
	SessionStore store;
	ExporterRegistry exporters;
	FakeGateway gateway;
	SessionController controller(gateway, config, store, exporters);
	controller.initialize();
	gateway.value.path = (dir + "/collision.mkv").toStdString();
	for (const auto *ext : {"csv", "edl", "srt", "vtt", "chapters.txt", "md", "xml"}) {
		QFile foreign(dir + "/collision." + ext);
		CHECK(foreign.open(QIODevice::WriteOnly));
		CHECK(foreign.write("foreign data") == 12);
	}
	gateway.start();
	CHECK(controller.trigger_quick_marker(0, "=HYPERLINK(\"https://invalid\")"));
	CHECK(controller.trigger_quick_marker(0, "+SUM(1,2)"));
	CHECK(controller.trigger_quick_marker(0, "-1+2"));
	CHECK(controller.trigger_quick_marker(0, "@formula"));
	CHECK(controller.trigger_quick_marker(0, "\t =formula"));
	CHECK(controller.trigger_quick_marker(0, "'=literal"));
	CHECK(controller.delete_marker(1));
	const auto session_id = controller.session().session_id();
	const auto original = controller.session().get_markers().front().comment;
	gateway.stop();
	for (const auto *ext : {"csv", "edl", "srt", "vtt", "chapters.txt", "md", "xml"}) {
		QFile foreign(dir + "/collision." + ext);
		CHECK(foreign.open(QIODevice::ReadOnly) && foreign.readAll() == "foreign data");
		CHECK(QFile::exists(dir + "/collision.timestamp-memo." + QString::fromStdString(session_id) + "." +
				    ext));
	}
	const auto csv_path = dir + "/collision.timestamp-memo." + QString::fromStdString(session_id) + ".csv";
	QFile csv(csv_path);
	CHECK(csv.open(QIODevice::ReadOnly));
	const auto bytes = csv.readAll();
	CHECK(bytes.startsWith("\xEF\xBB\xBFIndex,Marker ID,"));
	CHECK(bytes.contains("\n1,2,") && bytes.contains("\"'+SUM(1,2)\""));
	CHECK(bytes.contains("\"'-1+2\"") && bytes.contains("\"'@formula\""));
	CHECK(bytes.contains("\"'\t =formula\"") && bytes.contains("\"''=literal\""));
	CHECK(controller.session().get_markers().front().comment == original);
	CHECK(controller.perform_auto_export(gateway.value.path));
	CHECK(QFile::exists(dir + "/collision.timestamp-memo." + QString::fromStdString(session_id) + ".2.csv"));
	CHECK(config.save(PluginSettings{}));
}

static void config_migration(const QString &dir)
{
	PluginConfig legacy(QSettings::IniFormat);
	auto settings = PluginSettings{};
	settings.marker_types[0].label = "migrated 日本語";
	settings.auto_export.json = false;
	settings.show_status_bar_notification = false;
	CHECK(legacy.save(settings));
	const auto path = dir + "/portable-plugin/settings.ini";
	PluginConfig migrated(path.toStdString(), QSettings::IniFormat);
	CHECK(migrated.load() && QFile::exists(path));
	CHECK(migrated.values().marker_types[0].label == "migrated 日本語");
	CHECK(!migrated.values().auto_export.json && !migrated.values().show_status_bar_notification);
	CHECK(legacy.save(PluginSettings{}));
	PluginConfig restarted(path.toStdString(), QSettings::IniFormat);
	CHECK(restarted.load() && restarted.values().marker_types[0].label == "migrated 日本語");
	auto changed = restarted.values();
	changed.marker_types[0].label = "plugin-global";
	CHECK(restarted.save(changed));
	CHECK(legacy.load() && legacy.values().marker_types[0].label == "Marker 1");
	PluginConfig other((dir + "/other-portable/settings.ini").toStdString(), QSettings::IniFormat);
	CHECK(other.load() && other.values().marker_types[0].label == "Marker 1");
	QFile blocked(dir + "/blocked-migration-parent");
	CHECK(blocked.open(QIODevice::WriteOnly));
	blocked.close();
	PluginConfig failed((blocked.fileName() + "/settings.ini").toStdString(), QSettings::IniFormat);
	CHECK(!failed.load() && failed.values().marker_types[0].label == "Marker 1");
	PluginConfig unavailable(std::string{});
	CHECK(!unavailable.load() && !unavailable.save(changed));
	CHECK(legacy.save(PluginSettings{}));
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
		edit_history_and_recovery(dir.path());
		discard_documents(dir.path());
		safe_exports(dir.path());
		config_migration(dir.path());
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
		gateway.snapshots = 0;
		gateway.quick(1);
		CHECK(gateway.snapshots == 1);
		CHECK(controller.session().get_markers().size() == 1);
		CHECK(controller.session().get_markers().front().frame_index == 60);
		CHECK(controller.session().get_markers().front().label == "Custom");
		const auto custom_id = controller.session().get_markers().front().id;
		CHECK(controller.update_marker_data(custom_id, "Hand edited label", "", "memo"));
		CHECK(controller.update_marker_type(custom_id, 3));
		CHECK(controller.session().get_markers().front().label == "Hand edited label");
		CHECK(controller.session().get_markers().front().color == "#e74c3c");
		CHECK(controller.update_marker_data(custom_id, "Cut / Edit", "", "memo"));
		CHECK(controller.update_marker_type(custom_id, 1));
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
		codec_validation(dir.path());
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
