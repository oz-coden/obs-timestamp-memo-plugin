#include "session-controller.hpp"
#include "exporter-registry.hpp"
#include "plugin-config.hpp"
#include "session-codec.hpp"
#include <QDateTime>
#include <QUuid>

#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QEvent>

SessionController::SessionController(RecordingGateway &bridge, PluginConfig &config, SessionStore &store,
				     ExporterRegistry &exporters)
	: bridge_(bridge),
	  config_(config),
	  store_(store),
	  exporters_(exporters)
{
	poll_timer_.setInterval(250);
	connect(&poll_timer_, &QTimer::timeout, this, &SessionController::check_recording_file_changed);
}

SessionController::~SessionController()
{
	shutdown();
}

void SessionController::initialize()
{
	if (initialized_)
		return;

	auto &bridge = bridge_;
	if (!config_.load())
		notify("Warning: Settings could not be loaded. Current defaults remain active.", 5000);

	connect(&bridge, &RecordingGateway::recordingStarted, this, &SessionController::onRecordingStarted);
	connect(&bridge, &RecordingGateway::recordingPaused, this, &SessionController::onRecordingPaused);
	connect(&bridge, &RecordingGateway::recordingUnpaused, this, &SessionController::onRecordingUnpaused);
	connect(&bridge, &RecordingGateway::recordingStopped, this, &SessionController::onRecordingStopped);
	connect(&bridge, &RecordingGateway::recordingFileChanged, this, &SessionController::onRecordingFileSplit);
	connect(&bridge, &RecordingGateway::focusMemoRequested, this, &SessionController::focusMemoInputRequested);

	connect(&bridge, &RecordingGateway::quickMarkerRequested, this,
		[this](int index) { trigger_quick_marker(index); });

	initialized_ = true;
	bridge.initialize();
	poll_timer_.start();

	if (bridge.snapshot().recording) {
		onRecordingStarted();
	}
}

void SessionController::shutdown()
{
	if (!initialized_)
		return;

	auto &bridge = bridge_;
	bridge.drain_pending_events();
	initialized_ = false;
	poll_timer_.stop();
	disconnect(&bridge, nullptr, this, nullptr);
	bridge.shutdown();
	QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
	if (timeline_.active()) {
		std::string path = session_.video_path();
		if (!store_.finish(session_, config_.values().auto_export.json))
			notify("Warning: Failed to persist session during shutdown.", 5000);
		perform_auto_export(path);
	}
	timeline_.stop();
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
	uint64_t frames = bridge_.snapshot().total_frames;
	return TimecodeHelper::frame_index_to_ms(timeline_.relative_frames(frames), session_.frame_rate());
}

std::string SessionController::current_video_path() const
{
	return session_.video_path();
}

bool SessionController::trigger_quick_marker(int type_index, const std::string &comment)
{
	if (!initialized_ || !timeline_.active()) {
		notify("Not recording! Cannot stamp marker.", 2000);
		return false;
	}

	check_recording_file_changed();
	if (!timeline_.active())
		return false;

	if (type_index < 0 || type_index >= 4) {
		type_index = 0;
	}

	const auto &cfg = config_.values();
	std::string label = cfg.marker_types[type_index].label;
	std::string color = cfg.marker_types[type_index].color;
	bool paused = is_paused();

	MemoMarker m = session_.add_marker_at_frame(
		timeline_.relative_frames(bridge_.snapshot().total_frames), type_index, label, color, comment, paused,
		QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString());
	store_.append({{"op", "add"}, {"marker", SessionCodec::encode_marker(m)}});
	int row = static_cast<int>(session_.get_markers().size()) - 1;

	std::string tc = m.active_timecode(session_.frame_rate());
	std::string msg = "[" + label + "] " + tc + (paused ? " (Paused)" : "") + " Recorded";
	notify(msg, 3000);
	check_journal();

	emit markerAdded(m, row);
	return true;
}

bool SessionController::add_memo_marker(const std::string &comment, int type_index)
{
	return trigger_quick_marker(type_index, comment);
}

bool SessionController::update_marker_type(uint32_t id, int type)
{
	if (type < 0 || type >= 4)
		type = 0;
	for (auto candidate : session_.get_markers())
		if (candidate.id == id) {
			candidate.type_index = type;
			candidate.label = config_.values().marker_types[type].label;
			candidate.color = config_.values().marker_types[type].color;
			return apply_marker_update(candidate);
		}
	return false;
}
bool SessionController::update_marker_data(uint32_t id, const std::string &label, const std::string &color,
					   const std::string &comment)
{
	for (auto candidate : session_.get_markers())
		if (candidate.id == id) {
			candidate.label = label;
			candidate.comment = comment;
			if (!color.empty())
				candidate.color = color;
			return apply_marker_update(candidate);
		}
	return false;
}
bool SessionController::apply_marker_update(const MemoMarker &candidate)
{
	const auto &markers = session_.get_markers();
	for (size_t row = 0; row < markers.size(); ++row)
		if (markers[row].id == candidate.id) {
			if (!session_.update_marker(candidate.id, candidate.label, candidate.color, candidate.comment,
						    candidate.type_index))
				return false;
			journal_update(candidate.id);
			persist_edits();
			emit markerUpdated(candidate, static_cast<int>(row));
			return true;
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
		history_path_ = path;
		recovered_ = false;
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
		history_path_ = cache_path;
		recovered_ = true;
		emit markersReset(session_.get_markers(), session_.frame_rate());
		return true;
	}
	return false;
}

void SessionController::persist_edits()
{
	if (timeline_.active()) {
		check_journal();
	} else if (!store_.save(session_)) {
		notify("Warning: Failed to save edited markers. Export JSON to preserve changes.", 5000);
	}
}

void SessionController::check_journal()
{
	if (!store_.journal_healthy() && !journal_warning_shown_) {
		journal_warning_shown_ = true;
		notify("Warning: Recovery cache could not be written. Export JSON to preserve markers.", 5000);
	}
}

bool SessionController::perform_auto_export(const std::string &base_video_path)
{
	if (base_video_path.empty()) {
		const auto &a = config_.values().auto_export;
		return !(a.csv || a.edl || a.srt || a.vtt || a.youtube || a.markdown || a.xml);
	}

	QFileInfo fi(QString::fromStdString(base_video_path));
	QDir dir = fi.dir();
	QString base_name = fi.completeBaseName();

	const auto &auto_cfg = config_.values().auto_export;
	const auto &reg = exporters_;
	bool all_success = true;

	auto try_export = [&](bool enabled, const std::string &id, const std::string &ext) {
		if (!enabled)
			return;
		std::string path = dir.filePath(base_name + "." + QString::fromStdString(ext)).toStdString();
		if (!reg.export_by_id(id, session_, path)) {
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
		notify("Warning: Some auto-export formats failed to save!", 5000);
	}
	return all_success;
}

void SessionController::check_recording_file_changed()
{
	if (!is_recording())
		return;
	bridge_.drain_pending_events();

	std::string current_path = bridge_.snapshot().path;
	if (current_path.empty())
		return;

	if (session_.video_path().empty()) {
		session_.set_video_path(current_path);
		store_.append({{"op", "video_path"}, {"video_path", QString::fromStdString(current_path)}});
		check_journal();
		emit videoPathResolved(QString::fromStdString(current_path));
	}
}

void SessionController::onRecordingStarted()
{
	if (!initialized_ || timeline_.active())
		return;
	const auto capture = bridge_.snapshot();
	const auto &path = capture.path;
	const auto fps = capture.fps;
	const auto width = capture.width, height = capture.height;
	timeline_.start();
	start_document(path, fps, width, height);
	if (capture.paused)
		timeline_.pause();
	journal_warning_shown_ = false;

	emit markersReset({}, fps);
	emit sessionStarted(QString::fromStdString(path));
	notify("Recording started", 2000);
	check_journal();
}

void SessionController::onRecordingPaused()
{
	if (!timeline_.pause())
		return;
	emit sessionPaused();
	notify("Recording paused", 2000);
}

void SessionController::onRecordingUnpaused()
{
	if (!timeline_.resume())
		return;
	emit sessionResumed();
	notify("Recording resumed", 2000);
}

void SessionController::onRecordingStopped()
{
	if (!initialized_ || !timeline_.active())
		return;
	std::string video_path = session_.video_path();
	if (video_path.empty()) {
		video_path = bridge_.snapshot().path;
		if (!video_path.empty()) {
			session_.set_video_path(video_path);
			store_.append({{"op", "video_path"}, {"video_path", QString::fromStdString(video_path)}});
		}
	}

	std::string json_path;
	bool save_ok = store_.finish(session_, config_.values().auto_export.json, &json_path);
	timeline_.stop();

	bool export_ok = perform_auto_export(video_path);

	emit sessionStopped(QString::fromStdString(json_path));

	if (save_ok && export_ok) {
		notify("Recording stopped. Markers saved.", 3000);
	} else {
		notify("Recording stopped. Warning: Some marker files could not be saved. Check the OBS log.", 5000);
	}
}

void SessionController::onRecordingFileSplit(const QString &path, uint64_t frame_offset)
{
	if (!initialized_ || !timeline_.active() || path.isEmpty() || path.toStdString() == session_.video_path())
		return;
	if (!timeline_.split(frame_offset))
		return;
	std::string old_video_path = session_.video_path();
	std::string old_json_path;
	bool save_ok = store_.finish(session_, config_.values().auto_export.json, &old_json_path);
	bool export_ok = perform_auto_export(old_video_path);

	const auto capture = bridge_.snapshot();
	const auto new_path = path.toStdString();
	const auto fps = capture.fps;
	const auto width = capture.width, height = capture.height;
	start_document(new_path, fps, width, height);
	journal_warning_shown_ = false;

	emit markersReset({}, fps);
	emit videoPathResolved(QString::fromStdString(new_path));
	notify("Recording split into: " + new_path, 3000);
	check_journal();
	if (!save_ok || !export_ok)
		notify("Warning: Previous segment could not be saved. Check the recovery cache and OBS log.", 5000);
}

void SessionController::start_document(const std::string &path, const VideoFrameRate &fps, uint32_t width,
				       uint32_t height)
{
	history_path_.clear();
	recovered_ = false;
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

QString SessionController::document_title() const
{
	std::string path = history_path_.empty() ? session_.video_path() : history_path_;
	if (path.empty())
		return timeline_.active() ? "Resolving recording file..." : "No session loaded";
	return QFileInfo(QString::fromStdString(path)).fileName() +
	       (history_path_.empty() ? QString() : QString(recovered_ ? " (Recovered)" : " (Loaded)"));
}
