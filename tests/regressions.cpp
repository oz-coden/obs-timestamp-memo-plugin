#include "obs-stubs.hpp"
#include "dock-widget.hpp"
#include "settings-dialog.hpp"
#include "recording-session.hpp"
#include "session-store.hpp"
#include "session-codec.hpp"
#include <QDateTime>
#include <QUuid>
#include "session-controller.hpp"
#include "plugin-config.hpp"
#include "exporter-registry.hpp"
#include "export-dialog.hpp"
#include "marker-table-model.hpp"
#include "youtube-exporter.hpp"
#include "vtt-exporter.hpp"
#include "timecode-helper.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMenu>
#include <QGroupBox>
#include <QPointer>
#include <QLayout>
#include <QSettings>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
#include <thread>

bool obs_module_load();
void obs_module_unload();
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

static QByteArray read_file(const QString &path)
{
	QFile file(path);
	CHECK(file.open(QIODevice::ReadOnly));
	return file.readAll();
}
static void write_file(const QString &path, const QByteArray &bytes)
{
	QFile file(path);
	CHECK(file.open(QIODevice::WriteOnly));
	CHECK(file.write(bytes) == bytes.size());
}
static QStringList caches(const QString &dir)
{
	return QDir(dir).entryList({"*.tmp.jsonl"}, QDir::Files);
}
static void drain()
{
	QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
	QApplication::processEvents();
}

static void timecodes()
{
	for (const auto fps : {VideoFrameRate{30000, 1001}, VideoFrameRate{60000, 1001}, VideoFrameRate{24000, 1001},
			       VideoFrameRate{60, 1}}) {
		for (uint64_t frame : {0ULL, 1797ULL, 1798ULL, 1800ULL, 17982ULL, 35964ULL, 107892ULL, 215784ULL}) {
			CHECK(TimecodeHelper::smpte_to_frame_index(TimecodeHelper::frame_index_to_smpte(frame, fps),
								   fps) == frame);
			CHECK(TimecodeHelper::smpte_to_frame_index(
				      TimecodeHelper::frame_index_to_smpte(frame, fps, true), fps) == frame);
		}
	}
	CHECK(TimecodeHelper::frame_index_to_smpte(1800, {30000, 1001}) == "00:01:00;02");
	CHECK(TimecodeHelper::smpte_to_frame_index("00:01:00;00", {30000, 1001}) == 0);
	CHECK(TimecodeHelper::smpte_to_frame_index("00:60:00:00", {60, 1}) == 0);
	CHECK(TimecodeHelper::smpte_to_frame_index("00:00:00:60", {60, 1}) == 0);
	CHECK(TimecodeHelper::smpte_to_frame_index("00:00:01:ab", {60, 1}) == 0);
	CHECK(TimecodeHelper::smpte_to_frame_index("00:00:01:01junk", {60, 1}) == 0);
	std::cout << "PASS timecode DF/NDF boundaries and invalid inputs\n";
}

static bool begin(SessionStore &store, RecordingSession &session, const std::string &path, VideoFrameRate fps,
		  uint32_t width, uint32_t height)
{
	session.replace({path, QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
			 QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(), fps, width,
			 height});
	return store.begin(session);
}

static void persistence(const QString &dir)
{
	for (const auto fps : {VideoFrameRate{30000, 1001}, VideoFrameRate{60000, 1001}, VideoFrameRate{24000, 1001}}) {
		QString video = dir + "/録画-" + QString::number(fps.num) + ".mkv";
		RecordingSession session;
		SessionStore session_store((dir + "/fallback").toStdString());
		CHECK(begin(session_store, session, video.toStdString(), fps, 2560, 1440));
		auto marker = session.add_marker(1000, 0, "日本語", "#3498db", "comment", false);
		CHECK(session.update_marker(marker.id, "Cut", "#e74c3c", "changed", 3));
		CHECK(session_store.append(
			{{"op", "add"}, {"marker", SessionCodec::encode_marker(session.get_markers().front())}}));
		std::string path;
		CHECK(session_store.finish(session, true, &path));
		CHECK(!path.empty());
		RecordingSession loaded;
		SessionStore loaded_store((dir + "/fallback").toStdString());
		CHECK(loaded_store.load(path, loaded));
		CHECK(loaded.frame_rate().num == fps.num && loaded.frame_rate().den == fps.den);
		CHECK(loaded.get_markers().at(0).type_index == 3);
		CHECK(loaded.get_markers().at(0).timecode_df == session.get_markers().at(0).timecode_df);
		QString alias = dir + "/別名-" + QString::number(fps.num) + ".json";
		write_file(alias, read_file(QString::fromStdString(path)));
		QByteArray original = read_file(QString::fromStdString(path));
		CHECK(loaded_store.load(alias.toStdString(), loaded));
		CHECK(loaded.update_marker(marker.id, "Updated", "#ffffff", "saved to source"));
		CHECK(loaded_store.save(loaded));
		CHECK(read_file(QString::fromStdString(path)) == original);
		CHECK(read_file(alias).contains("saved to source"));
		write_file(dir + "/bad.json", "{}");
		auto before = SessionCodec::encode(loaded);
		CHECK(!loaded_store.load((dir + "/bad.json").toStdString(), loaded));
		CHECK(SessionCodec::encode(loaded) == before);
	}
	std::cout << "PASS fractional FPS, type persistence, Unicode and source JSON isolation\n";
}

static void journaling(const QString &dir)
{
	RecordingSession first, second;
	SessionStore first_store, second_store;
	QString video = dir + "/reused.mkv";
	CHECK(begin(first_store, first, video.toStdString(), {60000, 1001}, 1920, 1080));
	auto marker = first.add_marker(1001, 0, "First", "#3498db", "", false);
	CHECK(first.update_marker(marker.id, "Type 3", "#e74c3c", "persisted", 3));
	CHECK(first_store.append(
		{{"op", "add"}, {"marker", SessionCodec::encode_marker(first.get_markers().front())}}));
	CHECK(first_store.finish(first, false));
	QStringList before = caches(dir);
	CHECK(before.size() == 1);
	QString first_cache = dir + "/" + before.front();
	QByteArray original = read_file(first_cache);
	CHECK(begin(second_store, second, video.toStdString(), {60000, 1001}, 1920, 1080));
	CHECK(caches(dir).size() == 2);
	CHECK(read_file(first_cache) == original);
	CHECK(second_store.finish(second, false));
	CHECK(!QFile::exists(dir + "/reused.json"));
	RecordingSession recovered;
	SessionStore recovered_store((dir + "/fallback").toStdString());
	CHECK(recovered_store.recover(first_cache.toStdString(), recovered));
	CHECK(recovered.get_markers().at(0).type_index == 3);
	CHECK(recovered.frame_rate().num == 60000 && recovered.frame_rate().den == 1001);
	CHECK(recovered_store.save(recovered));
	CHECK(!QFile::exists(first_cache));
	CHECK(caches(dir).size() == 1);
	write_file(dir + "/torn.tmp.jsonl", original + "{\"op\":");
	CHECK(recovered_store.recover((dir + "/torn.tmp.jsonl").toStdString(), recovered));
	CHECK(recovered.get_markers().size() == 1);
	auto recovered_before = SessionCodec::encode(recovered);
	write_file(dir + "/corrupt.tmp.jsonl", original + "{broken}\n");
	CHECK(!recovered_store.recover((dir + "/corrupt.tmp.jsonl").toStdString(), recovered));
	CHECK(SessionCodec::encode(recovered) == recovered_before);

	RecordingSession delayed;
	SessionStore delayed_store((dir + "/fallback").toStdString());
	CHECK(begin(delayed_store, delayed, "", {30000, 1001}, 1920, 1080));
	auto early = delayed.add_marker(1000, 0, "Before path", "#3498db", "early", false);
	CHECK(delayed_store.append({{"op", "add"}, {"marker", SessionCodec::encode_marker(early)}}));
	QString fallback = dir + "/fallback";
	CHECK(caches(fallback).size() == 1);
	QString fallback_cache = fallback + "/" + caches(fallback).front();
	delayed.set_video_path((dir + "/late.mkv").toStdString());
	CHECK(delayed_store.append(
		{{"op", "video_path"}, {"video_path", QString::fromStdString(delayed.video_path())}}));
	CHECK(recovered_store.recover(fallback_cache.toStdString(), recovered));
	CHECK(recovered.get_markers().at(0).comment == "early");
	CHECK(recovered.video_path() == (dir + "/late.mkv").toStdString());
	CHECK(delayed_store.finish(delayed, true));
	CHECK(!QFile::exists(fallback_cache));

	RecordingSession failed;
	SessionStore failed_store((dir + "/fallback").toStdString());
	write_file(dir + "/blocked", "existing file");
	CHECK(!begin(failed_store, failed, (dir + "/blocked/video.mkv").toStdString(), {60, 1}, 1920, 1080));
	CHECK(!failed_store.journal_healthy());
	failed.add_marker(0, 0, "In memory", "", "", false);
	CHECK(!failed_store.finish(failed, true));
	CHECK(failed.get_markers().size() == 1);
	std::cout << "PASS unique journals, JSON disabled, delayed path recovery and I/O failure retention\n";
}

static void exports(const QString &dir)
{
	RecordingSession session;
	SessionStore session_store((dir + "/fallback").toStdString());
	CHECK(begin(session_store, session, (dir + "/exports.mkv").toStdString(), {60000, 1001}, 1920, 1080));
	session.add_marker(2000, 1, "late", "#ffffff", "& <tag>\n\ntext", false);
	session.add_marker(0, 0, "早い", "#000000", "comma, quote\"", false);
	session.add_marker(2500, 0, "same second", "", "control\x01", true);
	RecordingSession snapshot(session);
	CHECK(session_store.finish(session, true));
	session.clear_markers();
	CHECK(snapshot.get_markers().size() == 3);
	ExporterRegistry catalogue;
	auto registry = catalogue.get_all();
	CHECK(registry.size() == 8);
	for (const auto &exporter : registry) {
		QString path = dir + "/書出し." + QString::fromStdString(exporter->get_file_extension());
		CHECK(exporter->export_to_file(snapshot, path.toStdString()));
		CHECK(!read_file(path).isEmpty());
		CHECK(!exporter->export_to_file(
			snapshot, (dir + "/blocked/fail." + QString::fromStdString(exporter->get_file_extension()))
					  .toStdString()));
	}
	CHECK(read_file(dir + "/書出し.csv").startsWith(QByteArray::fromHex("efbbbf")));
	CHECK(read_file(dir + "/書出し.srt").startsWith(QByteArray::fromHex("efbbbf")));
	CHECK(!read_file(dir + "/書出し.xml").contains('\x01'));
	auto chapters = YoutubeExporter::generate_chapters(snapshot);
	CHECK(chapters.starts_with("00:00 "));
	CHECK(chapters.find("00:02 late") != std::string::npos);
	CHECK(chapters.find(" / same second") != std::string::npos);
	auto vtt = VttExporter().export_to_string(snapshot);
	CHECK(vtt.find("&amp; &lt;tag&gt;") != std::string::npos);
	CHECK(vtt.find("00:00:00.000") < vtt.find("00:00:02.000"));
	CHECK(vtt.find("<tag>") == std::string::npos);
	QPointer<QLayout> layout_guard;
	{
		ExportDialog dialog(snapshot, catalogue);
		layout_guard = dialog.findChild<QGroupBox *>()->layout();
		snapshot.clear_markers();
		dialog.findChild<QButtonGroup *>()->button(5)->setChecked(true);
		CHECK(QMetaObject::invokeMethod(&dialog, "onCopyClicked", Qt::DirectConnection));
		CHECK(QApplication::clipboard()->text().contains("same second"));
		CHECK(QApplication::activeModalWidget() == nullptr);
	}
	CHECK(layout_guard.isNull());
	std::cout << "PASS all 8 exporters, snapshot, BOM, ordering and escaped VTT\n";
}

static void model_reentrancy()
{
	MarkerTableModel model;
	model.add_marker(MemoMarker::create(1, 0, {60, 1}, 0, "old", "", "", false), {60, 1});
	uint32_t emitted_id = 0;
	CHECK(model.setData(model.index(0, MarkerTableModel::Col_Label), "unapplied"));
	CHECK(model.data(model.index(0, MarkerTableModel::Col_Label)).toString() == "old");
	QObject::connect(&model, &MarkerTableModel::markerDataChanged, &model,
			 [&model](uint32_t, const QString &, const QString &) { model.clear(); });
	QObject::connect(&model, &MarkerTableModel::markerDataChanged, &model,
			 [&emitted_id](uint32_t id, const QString &, const QString &) { emitted_id = id; });
	CHECK(model.setData(model.index(0, MarkerTableModel::Col_Label), "new"));
	CHECK(emitted_id == 1);
	CHECK(model.rowCount() == 0);
	std::cout << "PASS model signal reentrancy\n";
}

static void lifecycle_and_ui(const QString &dir, QMainWindow &window)
{
	FakeObs::reset(&window, dir + "/fallback");
	CHECK(obs_module_load());
	CHECK(obs_module_load());
	CHECK(FakeObs::callbacks() == 2 && FakeObs::save_callbacks() == 1 && FakeObs::hotkeys() == 5);
	CHECK(FakeObs::dock());
	CHECK(!qobject_cast<QDockWidget *>(FakeObs::dock()->widget()));
	CHECK(FakeObs::dock()->findChildren<QDockWidget *>().empty());
	auto *table = FakeObs::dock()->findChild<QTableView *>();
	CHECK(table);
	FakeObs::dock()->show();
	window.show();
	drain();
	auto &controller = qobject_cast<DockWidget *>(FakeObs::dock()->widget())->controller();
	CHECK(!controller.trigger_quick_marker(0));
	FakeObs::set_rate(60000, 1001);
	FakeObs::start(dir + "/segment1.mkv", 600);
	CHECK(controller.trigger_quick_marker(0));
	CHECK(table->model()->rowCount() == 1);
	CHECK(table->model()->setData(table->model()->index(0, MarkerTableModel::Col_Comment), "UI edit"));
	CHECK(controller.session().get_markers().front().comment == "UI edit");
	CHECK(table->model()->data(table->model()->index(0, MarkerTableModel::Col_Comment)).toString() == "UI edit");
	uint32_t id = controller.session().get_markers().at(0).id;
	CHECK(controller.update_marker_type(id, 3));
	CHECK(controller.session().get_markers().at(0).type_index == 3);
	FakeObs::set_paused(true);
	CHECK(controller.trigger_quick_marker(1));
	CHECK(controller.session().get_markers().back().is_paused);
	FakeObs::set_paused(false);
	std::thread split_thread([&dir]() { FakeObs::split(dir + "/segment2.mkv", 600); });
	split_thread.join();
	CHECK(controller.session().video_path() == (dir + "/segment1.mkv").toStdString());
	drain();
	CHECK(controller.session().video_path() == (dir + "/segment2.mkv").toStdString());
	CHECK(controller.current_record_ms() == 0);
	FakeObs::set_frames(660);
	CHECK(controller.trigger_quick_marker(0));
	CHECK(controller.session().get_markers().at(0).timestamp_ms == 1001);
	CHECK(read_file(dir + "/segment1.json").contains("Cut / Edit"));
	auto *content = qobject_cast<DockWidget *>(FakeObs::dock()->widget());
	CHECK(QMetaObject::invokeMethod(content, "onExportClicked", Qt::DirectConnection));
	CHECK(QMetaObject::invokeMethod(content, "onExportClicked", Qt::DirectConnection));
	CHECK(content->findChildren<ExportDialog *>().size() == 1);
	CHECK(content->findChild<ExportDialog *>()->windowModality() == Qt::NonModal);
	CHECK(QMetaObject::invokeMethod(content, "onSettingsClicked", Qt::DirectConnection));
	CHECK(QMetaObject::invokeMethod(content, "onSettingsClicked", Qt::DirectConnection));
	CHECK(content->findChildren<SettingsDialog *>().size() == 1);
	CHECK(QApplication::activeModalWidget() == nullptr);
	CHECK(FakeObs::output_refs() == 1 && FakeObs::file_callbacks() == 1);

	QTimer::singleShot(0, [&controller]() {
		auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
		CHECK(menu);
		controller.clear_markers();
		for (int i = 0; i < 40; ++i)
			controller.trigger_quick_marker(0);
		menu->setActiveAction(menu->actions().front());
		QKeyEvent key(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
		QApplication::sendEvent(menu, &key);
	});
	QPoint point = table->visualRect(table->model()->index(0, 0)).center();
	CHECK(QMetaObject::invokeMethod(FakeObs::dock()->widget(), "onContextMenuRequested", Qt::DirectConnection,
					Q_ARG(QPoint, point)));
	CHECK(QApplication::clipboard()->text() == "00:00:01;00");
	FakeObs::stop();
	CHECK(FakeObs::output_refs() == 0 && FakeObs::file_callbacks() == 0);
	FakeObs::start(dir + "/pending1.mkv");
	CHECK(controller.trigger_quick_marker(0));
	FakeObs::split(dir + "/pending2.mkv", 600);
	FakeObs::stop();
	CHECK(controller.session().video_path() == (dir + "/pending2.mkv").toStdString());
	CHECK(QFile::exists(dir + "/pending1.json") && QFile::exists(dir + "/pending2.json"));
	QPointer<DockWidget> removed_content = qobject_cast<DockWidget *>(FakeObs::dock()->widget());
	obs_module_unload();
	CHECK(removed_content.isNull());
	CHECK(FakeObs::callbacks() == 0 && FakeObs::hotkeys() == 0);
	CHECK(obs_module_load());
	auto &reloaded = qobject_cast<DockWidget *>(FakeObs::dock()->widget())->controller();
	FakeObs::start(dir + "/reload.mkv");
	FakeObs::hotkey(0);
	CHECK(reloaded.session().get_markers().size() == 1);
	CHECK(FakeObs::dock()->findChild<QTableView *>()->model()->rowCount() == 1);
	FakeObs::stop();
	FakeObs::set_encoder(2, 1280, 720);
	FakeObs::start(dir + "/divisor.mkv", 30);
	CHECK(reloaded.trigger_quick_marker(0));
	CHECK(reloaded.session().frame_rate().num == 30000 && reloaded.session().frame_rate().den == 1001);
	CHECK(reloaded.session().get_markers().at(0).timestamp_ms == 1001);
	CHECK(reloaded.session().width() == 1280 && reloaded.session().height() == 720);
	FakeObs::exit();
	drain();
	obs_module_unload();
	CHECK(FakeObs::invalid_frontend_calls() == 0);
	CHECK(FakeObs::callbacks() == 0 && FakeObs::save_callbacks() == 0 && FakeObs::hotkeys() == 0);
	CHECK(FakeObs::output_refs() == 0 && FakeObs::file_callbacks() == 0);
	CHECK(!FakeObs::dock());
	CHECK(QFile::exists(dir + "/divisor.json"));
	FakeObs::reset(&window, dir + "/fallback");
	FakeObs::reject_dock(true);
	CHECK(!obs_module_load());
	CHECK(FakeObs::callbacks() == 0 && FakeObs::save_callbacks() == 0 && FakeObs::hotkeys() == 0);
	std::cout << "PASS UI hierarchy, context menu mutation, paused markers, split, reload, EXIT and dock failure\n";
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	QTemporaryDir dir(QDir::currentPath() + "/regression-data-XXXXXX");
	if (!dir.isValid())
		return 1;
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path() + "/settings");
	QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, dir.path() + "/settings");
	QMainWindow window;
	FakeObs::reset(&window, dir.path() + "/fallback");
	try {
		timecodes();
		persistence(dir.path());
		journaling(dir.path());
		exports(dir.path());
		model_reentrancy();
		lifecycle_and_ui(dir.path(), window);
		std::cout << "All regression groups passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << "\n";
		obs_module_unload();
		return 1;
	}
}
