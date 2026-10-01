#include "session-controller.hpp"
#include "exporter-registry.hpp"
#include "obs-bridge.hpp"
#include "plugin-config.hpp"
#include "status-notifier.hpp"

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
	if (session_.is_active()) {
		std::string path = session_.video_path();
		if (!session_.stop_session(nullptr, PluginConfig::instance().auto_export.json))
			obs_log(LOG_ERROR, "[Timestamp Memo] Failed to persist session during shutdown");
		perform_auto_export(path);
	}
	StatusNotifier::instance().shutdown();
}

bool SessionController::is_recording() const
{
	return ObsBridge::instance().is_recording();
}

bool SessionController::is_paused() const
{
	return ObsBridge::instance().is_paused();
}

uint64_t SessionController::current_record_ms() const
{
	uint64_t frames = ObsBridge::instance().get_current_record_frames();
	return TimecodeHelper::frame_index_to_ms(frames >= segment_frame_offset_ ? frames - segment_frame_offset_ : 0,
						 session_.frame_rate());
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
	if (!initialized_ || !is_recording() || !session_.is_active()) {
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
	uint64_t ms = current_record_ms();

	MemoMarker m = session_.add_marker(ms, type_index, label, color, comment, paused);
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
	persist_edits();
	emit markersReset({}, session_.frame_rate());
}

bool SessionController::load_from_json(const std::string &path)
{
	if (is_recording())
		return false;

	if (session_.load_from_json(path)) {
		emit markersReset(session_.get_markers(), session_.frame_rate());
		return true;
	}
	return false;
}

bool SessionController::recover_from_cache(const std::string &cache_path)
{
	if (is_recording())
		return false;

	if (RecordingSession::recover_from_cache(cache_path, session_)) {
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
	if (session_.is_active()) {
		check_journal();
	} else if (!session_.save_to_json()) {
		StatusNotifier::instance().notify(
			"Warning: Failed to save edited markers. Export JSON to preserve changes.", 5000);
	}
}

void SessionController::check_journal()
{
	if (!session_.journal_healthy() && !journal_warning_shown_) {
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
		emit videoPathResolved(QString::fromStdString(current_path));
	}
}

void SessionController::onRecordingStarted()
{
	if (!initialized_ || session_.is_active())
		return;
	auto &bridge = ObsBridge::instance();
	std::string path = bridge.get_current_record_file_path();
	VideoFrameRate fps = bridge.get_current_frame_rate();
	uint32_t width = 1920, height = 1080;
	bridge.get_video_dimension(width, height);

	session_.start_session(path, fps, width, height);
	segment_frame_offset_ = 0;
	journal_warning_shown_ = false;

	emit markersReset({}, fps);
	emit sessionStarted(QString::fromStdString(path));
	StatusNotifier::instance().notify("Recording started", 2000);
	check_journal();
}

void SessionController::onRecordingPaused()
{
	emit sessionPaused();
	StatusNotifier::instance().notify("Recording paused", 2000);
}

void SessionController::onRecordingUnpaused()
{
	emit sessionResumed();
	StatusNotifier::instance().notify("Recording resumed", 2000);
}

void SessionController::onRecordingStopped()
{
	if (!initialized_ || !session_.is_active())
		return;
	std::string video_path = session_.video_path();
	if (video_path.empty()) {
		video_path = ObsBridge::instance().get_current_record_file_path();
		if (!video_path.empty()) {
			session_.set_video_path(video_path);
		}
	}

	std::string json_path;
	bool save_ok = session_.stop_session(&json_path, PluginConfig::instance().auto_export.json);

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
	if (!initialized_ || !session_.is_active() || path.isEmpty() || path.toStdString() == session_.video_path())
		return;
	std::string old_video_path = session_.video_path();
	std::string old_json_path;
	bool save_ok = session_.stop_session(&old_json_path, PluginConfig::instance().auto_export.json);
	if (!save_ok) {
		obs_log(LOG_ERROR, "[Timestamp Memo] Failed to save previous segment JSON on file split");
	}
	bool export_ok = perform_auto_export(old_video_path);

	auto &bridge = ObsBridge::instance();
	std::string new_path = path.toStdString();
	VideoFrameRate fps = bridge.get_current_frame_rate();
	uint32_t width = 1920, height = 1080;
	bridge.get_video_dimension(width, height);

	session_.start_session(new_path, fps, width, height);
	segment_frame_offset_ = frame_offset;
	journal_warning_shown_ = false;

	emit markersReset({}, fps);
	emit videoPathResolved(QString::fromStdString(new_path));
	StatusNotifier::instance().notify("Recording split into: " + new_path, 3000);
	check_journal();
	if (!save_ok || !export_ok)
		StatusNotifier::instance().notify(
			"Warning: Previous segment could not be saved. Check the recovery cache and OBS log.", 5000);
}
