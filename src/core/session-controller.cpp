#include "session-controller.hpp"
#include "exporter-registry.hpp"
#include "obs-bridge.hpp"
#include "plugin-config.hpp"
#include "status-notifier.hpp"
#include "session-codec.hpp"
#include <QDateTime>
#include <QUuid>

#include <plugin-support.h>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QEvent>

SessionController &SessionController::instance()
{
	static SessionController inst;
	return inst;
}

SessionController::SessionController() {}

SessionController::~SessionController()
{
	shutdown();
}

void SessionController::initialize()
{
	if (initialized_)
		return;

	auto &bridge = ObsBridge::instance();
	PluginConfig::instance().load();
	StatusNotifier::instance().initialize();
	store_.set_cache_directory(bridge.recovery_cache_directory());

	connect(&bridge, &ObsBridge::obsRecordingStarted, this, &SessionController::onRecordingStarted);
	connect(&bridge, &ObsBridge::obsRecordingPaused, this, &SessionController::onRecordingPaused);
	connect(&bridge, &ObsBridge::obsRecordingUnpaused, this, &SessionController::onRecordingUnpaused);
	connect(&bridge, &ObsBridge::obsRecordingStopped, this, &SessionController::onRecordingStopped);
	connect(&bridge, &ObsBridge::obsRecordingFileChanged, this, &SessionController::onRecordingFileSplit);
	connect(&bridge, &ObsBridge::obsFocusMemoRequested, this, &SessionController::focusMemoInputRequested);

	connect(&bridge, &ObsBridge::obsTriggerMarker1, this, [this]() { trigger_quick_marker(0); });
	connect(&bridge, &ObsBridge::obsTriggerMarker2, this, [this]() { trigger_quick_marker(1); });
	connect(&bridge, &ObsBridge::obsTriggerMarker3, this, [this]() { trigger_quick_marker(2); });
	connect(&bridge, &ObsBridge::obsTriggerMarker4, this, [this]() { trigger_quick_marker(3); });

	initialized_ = true;
	bridge.initialize();
	obs_log(LOG_INFO, "[Timestamp Memo] SessionController initialized");

	if (bridge.is_recording()) {
		onRecordingStarted();
	}
}

void SessionController::shutdown()
{
	if (!initialized_)
		return;

	auto &bridge = ObsBridge::instance();
	QCoreApplication::sendPostedEvents(&bridge, QEvent::MetaCall);
	initialized_ = false;
	disconnect(&bridge, nullptr, this, nullptr);
	bridge.shutdown();
	QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
	if (timeline_.active()) {
		std::string path = session_.video_path();
		if (!store_.finish(session_, PluginConfig::instance().auto_export.json))
			obs_log(LOG_ERROR, "[Timestamp Memo] Failed to persist session during shutdown");
		perform_auto_export(path);
	}
	timeline_.stop();
	StatusNotifier::instance().shutdown();
}

bool SessionController::is_recording() const
{
	return timeline_.active();
}

bool SessionController::is_paused() const
{
	return timeline_.paused();
}

uint64_t SessionController::current_record_ms() const
{
	uint64_t frames = ObsBridge::instance().get_current_record_frames();
	return TimecodeHelper::frame_index_to_ms(timeline_.relative_frames(frames), session_.frame_rate());
}

VideoFrameRate SessionController::current_frame_rate() const
{
	return ObsBridge::instance().get_current_frame_rate();
}

std::string SessionController::current_video_path() const
{
	return session_.video_path();
}

bool SessionController::trigger_quick_marker(int type_index, const std::string &comment)
{
	if (!initialized_ || !is_recording() || !timeline_.active()) {
		StatusNotifier::instance().notify("Not recording! Cannot stamp marker.", 2000);
		return false;
	}

	check_recording_file_changed();

	if (type_index < 0 || type_index >= 4) {
		type_index = 0;
	}

	const auto &cfg = PluginConfig::instance();
	std::string label = cfg.marker_types[type_index].label;
	std::string color = cfg.marker_types[type_index].color;
	bool paused = is_paused();

	MemoMarker m = session_.add_marker_at_frame(
		timeline_.relative_frames(ObsBridge::instance().get_current_record_frames()), type_index, label, color,
		comment, paused, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString());
	store_.append({{"op", "add"}, {"marker", SessionCodec::encode_marker(m)}});
	int row = static_cast<int>(session_.get_markers().size()) - 1;

	std::string tc = m.active_timecode(session_.frame_rate());
	std::string msg = "[" + label + "] " + tc + (paused ? " (Paused)" : "") + " Recorded";
	StatusNotifier::instance().notify(msg, 3000);
	check_journal();

	emit markerAdded(m, row);
	return true;
}

bool SessionController::add_memo_marker(const std::string &comment, int type_index)
{
	return trigger_quick_marker(type_index, comment);
}

bool SessionController::update_marker_comment(uint32_t id, const std::string &comment)
{
	auto markers = session_.get_markers();
	for (size_t i = 0; i < markers.size(); ++i) {
		if (markers[i].id == id) {
			session_.update_marker(id, markers[i].label, markers[i].color, comment);
			markers[i].comment = comment;
			journal_update(id);
			persist_edits();
			emit markerUpdated(markers[i], static_cast<int>(i));
			return true;
		}
	}
	return false;
}

bool SessionController::update_marker_type(uint32_t id, int type_index)
{
	if (type_index < 0 || type_index >= 4)
		type_index = 0;

	const auto &cfg = PluginConfig::instance();
	std::string label = cfg.marker_types[type_index].label;
	std::string color = cfg.marker_types[type_index].color;

	auto markers = session_.get_markers();
	for (size_t i = 0; i < markers.size(); ++i) {
		if (markers[i].id == id) {
			session_.update_marker(id, label, color, markers[i].comment, type_index);
			markers[i].label = label;
			markers[i].color = color;
			markers[i].type_index = type_index;
			journal_update(id);
			persist_edits();
			emit markerUpdated(markers[i], static_cast<int>(i));
			return true;
		}
	}
	return false;
}

bool SessionController::update_marker_data(uint32_t id, const std::string &label, const std::string &color,
					   const std::string &comment)
{
	auto markers = session_.get_markers();
	for (size_t i = 0; i < markers.size(); ++i) {
		if (markers[i].id == id) {
			std::string final_color = color.empty() ? markers[i].color : color;
			session_.update_marker(id, label, final_color, comment);
			markers[i].label = label;
			markers[i].color = final_color;
			markers[i].comment = comment;
			journal_update(id);
			persist_edits();
			emit markerUpdated(markers[i], static_cast<int>(i));
			return true;
		}
	}
	return false;
}

bool SessionController::delete_marker(uint32_t id)
{
	auto markers = session_.get_markers();
	for (size_t i = 0; i < markers.size(); ++i) {
		if (markers[i].id == id) {
			session_.delete_marker(id);
			if (timeline_.active())
				store_.append({{"op", "delete"}, {"id", static_cast<qint64>(id)}});
			persist_edits();
			emit markerRemoved(id, static_cast<int>(i));
			return true;
		}
	}
	return false;
}

void SessionController::clear_markers()
{
	session_.clear_markers();
	if (timeline_.active())
		store_.append({{"op", "clear"}});
	persist_edits();
	emit markersReset({}, session_.frame_rate());
}

bool SessionController::load_from_json(const std::string &path)
{
	if (is_recording())
		return false;

	if (store_.load(path, session_)) {
		emit markersReset(session_.get_markers(), session_.frame_rate());
		return true;
	}
	return false;
}

bool SessionController::recover_from_cache(const std::string &cache_path)
{
	if (is_recording())
		return false;

	if (store_.recover(cache_path, session_)) {
		emit markersReset(session_.get_markers(), session_.frame_rate());
		return true;
	}
	return false;
}

void SessionController::save_current_session()
{
	persist_edits();
}

void SessionController::persist_edits()
{
	if (timeline_.active()) {
		check_journal();
	} else if (!store_.save(session_)) {
		StatusNotifier::instance().notify(
			"Warning: Failed to save edited markers. Export JSON to preserve changes.", 5000);
	}
}

void SessionController::check_journal()
{
	if (!store_.journal_healthy() && !journal_warning_shown_) {
		journal_warning_shown_ = true;
		StatusNotifier::instance().notify(
			"Warning: Recovery cache could not be written. Export JSON to preserve markers.", 5000);
	}
}

bool SessionController::perform_auto_export(const std::string &base_video_path)
{
	if (base_video_path.empty())
		return false;

	QFileInfo fi(QString::fromStdString(base_video_path));
	QDir dir = fi.dir();
	QString base_name = fi.completeBaseName();

	const auto &auto_cfg = PluginConfig::instance().auto_export;
	const auto &reg = ExporterRegistry::instance();
	bool all_success = true;

	auto try_export = [&](bool enabled, const std::string &id, const std::string &ext) {
		if (!enabled)
			return;
		std::string path = dir.filePath(base_name + "." + QString::fromStdString(ext)).toStdString();
		if (!reg.export_by_id(id, session_, path)) {
			obs_log(LOG_ERROR, "[Timestamp Memo] Failed to auto-export %s to: %s", id.c_str(),
				path.c_str());
			all_success = false;
		}
	};

	try_export(auto_cfg.csv, "csv", "csv");
	try_export(auto_cfg.edl, "edl", "edl");
	try_export(auto_cfg.srt, "srt", "srt");
	try_export(auto_cfg.vtt, "vtt", "vtt");
	try_export(auto_cfg.youtube, "youtube", "chapters.txt");
	try_export(auto_cfg.markdown, "markdown", "md");
	try_export(auto_cfg.xml, "xml", "xml");

	if (!all_success) {
		StatusNotifier::instance().notify("Warning: Some auto-export formats failed to save!", 5000);
	}
	return all_success;
}

void SessionController::check_recording_file_changed()
{
	if (!is_recording())
		return;
	QCoreApplication::sendPostedEvents(&ObsBridge::instance(), QEvent::MetaCall);

	std::string current_path = ObsBridge::instance().get_current_record_file_path();
	if (current_path.empty())
		return;

	if (session_.video_path().empty()) {
		obs_log(LOG_INFO, "[Timestamp Memo] Late video path resolution: '%s'", current_path.c_str());
		session_.set_video_path(current_path);
		store_.append({{"op", "video_path"}, {"video_path", QString::fromStdString(current_path)}});
		emit videoPathResolved(QString::fromStdString(current_path));
	}
}

void SessionController::onRecordingStarted()
{
	if (!initialized_ || timeline_.active())
		return;
	auto &bridge = ObsBridge::instance();
	std::string path = bridge.get_current_record_file_path();
	VideoFrameRate fps = bridge.get_current_frame_rate();
	uint32_t width = 1920, height = 1080;
	bridge.get_video_dimension(width, height);

	timeline_.start();
	start_document(path, fps, width, height);
	if (bridge.is_paused())
		timeline_.pause();
	journal_warning_shown_ = false;

	emit markersReset({}, fps);
	emit sessionStarted(QString::fromStdString(path));
	StatusNotifier::instance().notify("Recording started", 2000);
	check_journal();
}

void SessionController::onRecordingPaused()
{
	if (!timeline_.pause())
		return;
	emit sessionPaused();
	StatusNotifier::instance().notify("Recording paused", 2000);
}

void SessionController::onRecordingUnpaused()
{
	if (!timeline_.resume())
		return;
	emit sessionResumed();
	StatusNotifier::instance().notify("Recording resumed", 2000);
}

void SessionController::onRecordingStopped()
{
	if (!initialized_ || !timeline_.active())
		return;
	std::string video_path = session_.video_path();
	if (video_path.empty()) {
		video_path = ObsBridge::instance().get_current_record_file_path();
		if (!video_path.empty()) {
			session_.set_video_path(video_path);
			store_.append({{"op", "video_path"}, {"video_path", QString::fromStdString(video_path)}});
		}
	}

	std::string json_path;
	bool save_ok = store_.finish(session_, PluginConfig::instance().auto_export.json, &json_path);
	timeline_.stop();

	bool export_ok = perform_auto_export(video_path);

	emit sessionStopped(QString::fromStdString(json_path));

	if (save_ok && export_ok) {
		StatusNotifier::instance().notify("Recording stopped. Markers saved.", 3000);
	} else {
		StatusNotifier::instance().notify(
			"Recording stopped. Warning: Some marker files could not be saved. Check the OBS log.", 5000);
	}
}

void SessionController::onRecordingFileSplit(const QString &path, uint64_t frame_offset)
{
	if (!initialized_ || !timeline_.active() || path.isEmpty() || path.toStdString() == session_.video_path())
		return;
	std::string old_video_path = session_.video_path();
	std::string old_json_path;
	bool save_ok = store_.finish(session_, PluginConfig::instance().auto_export.json, &old_json_path);
	if (!save_ok) {
		obs_log(LOG_ERROR, "[Timestamp Memo] Failed to save previous segment JSON on file split");
	}
	bool export_ok = perform_auto_export(old_video_path);

	auto &bridge = ObsBridge::instance();
	std::string new_path = path.toStdString();
	VideoFrameRate fps = bridge.get_current_frame_rate();
	uint32_t width = 1920, height = 1080;
	bridge.get_video_dimension(width, height);

	timeline_.split(frame_offset);
	start_document(new_path, fps, width, height);
	journal_warning_shown_ = false;

	emit markersReset({}, fps);
	emit videoPathResolved(QString::fromStdString(new_path));
	StatusNotifier::instance().notify("Recording split into: " + new_path, 3000);
	check_journal();
	if (!save_ok || !export_ok)
		StatusNotifier::instance().notify(
			"Warning: Previous segment could not be saved. Check the recovery cache and OBS log.", 5000);
}

void SessionController::start_document(const std::string &path, const VideoFrameRate &fps, uint32_t width,
				       uint32_t height)
{
	session_.replace({path, QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
			  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(), fps, width,
			  height});
	store_.begin(session_);
}
void SessionController::journal_update(uint32_t id)
{
	if (!timeline_.active())
		return;
	for (const auto &m : session_.get_markers())
		if (m.id == id) {
			auto op = SessionCodec::encode_marker(m);
			op["op"] = "update";
			store_.append(op);
			break;
		}
}
