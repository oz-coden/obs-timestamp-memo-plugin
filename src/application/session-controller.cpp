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
#include <algorithm>

bool SessionController::preserve_current_document()
{
	if (!document_unsaved_)
		return true;
	UnsavedDocument document;
	document.title = QFileInfo(QString::fromStdString(session_.video_path())).fileName().toStdString();
	if (document.title.empty())
		document.title = "Unresolved recording (" + session_.session_id().substr(0, 8) + ")";
	document.journal_path = store_.cache_path();
	if (journal_current_ && store_.journal_healthy() &&
	    QFile::exists(QString::fromStdString(document.journal_path)))
		document.recovery_path = document.journal_path;
	else
		document.recovery_path = store_.create_recovery_copy(session_);
	if (document.recovery_path.empty()) {
		// Disk-backed entries do not retain snapshots. If all storage fails,
		// keep at most eight snapshots plus the current document, never evict data.
		const auto count = std::count_if(unsaved_documents_.begin(), unsaved_documents_.end(),
						 [](const auto &entry) { return entry.memory.has_value(); });
		if (count >= 8) {
			warn("Unsaved document limit reached. OBS recording continues, but marker capture is suspended. "
			     "Use Unsaved to save a document and resume capture.",
			     10000);
			return false;
		}
		document.memory = session_;
	}
	document.id = next_unsaved_id_++;
	unsaved_documents_.push_back(std::move(document));
	document_unsaved_ = false;
	emit unsavedDocumentsChanged();
	return true;
}

bool SessionController::read_unsaved_document(uint64_t id, RecordingSession &document) const
{
	for (const auto &entry : unsaved_documents_) {
		if (entry.id != id)
			continue;
		if (entry.memory) {
			document = *entry.memory;
			return true;
		}
		SessionStore reader;
		return QString::fromStdString(entry.recovery_path).endsWith(".tmp.jsonl")
			       ? reader.recover(entry.recovery_path, document)
			       : reader.load(entry.recovery_path, document);
	}
	return false;
}

bool SessionController::save_unsaved_document(uint64_t id, const std::string &path)
{
	RecordingSession document;
	if (path.empty() || !read_unsaved_document(id, document) || !exporters_.export_by_id("json", document, path))
		return false;
	auto it = std::find_if(unsaved_documents_.begin(), unsaved_documents_.end(),
			       [id](const auto &entry) { return entry.id == id; });
	if (it == unsaved_documents_.end())
		return false;
	for (const auto &cache : {it->recovery_path, it->journal_path})
		if (!cache.empty() && QFileInfo(QString::fromStdString(cache)).canonicalFilePath() !=
					      QFileInfo(QString::fromStdString(path)).canonicalFilePath())
			QFile::remove(QString::fromStdString(cache));
	unsaved_documents_.erase(it);
	resume_capture();
	emit unsavedDocumentsChanged();
	return true;
}

bool SessionController::save_current_document(const std::string &path)
{
	if (path.empty() || (is_recording() && !capture_blocked_) || !exporters_.export_by_id("json", session_, path))
		return false;
	store_.acknowledge_saved(path);
	document_unsaved_ = false;
	resume_capture();
	emit unsavedDocumentsChanged();
	return true;
}

void SessionController::resume_capture()
{
	if (!capture_blocked_ || !timeline_.active())
		return;
	const auto capture = pending_capture_;
	start_document(capture.path, capture.fps, capture.width, capture.height);
	if (!capture_blocked_) {
		emit markersReset(session_.get_markers(), session_.frame_rate());
		emit videoPathResolved(QString::fromStdString(capture.path));
		notify("Marker capture resumed. Earlier unsaved documents remain available in Unsaved.", 5000);
	}
}

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
		warn("Warning: Settings could not be loaded. Current defaults remain active.", 5000);
	for (const auto &path : store_.recovery_copies()) {
		if (std::any_of(unsaved_documents_.begin(), unsaved_documents_.end(),
				[&path](const auto &entry) { return entry.recovery_path == path; }))
			continue;
		RecordingSession document;
		SessionStore reader;
		const auto title =
			reader.load(path, document)
				? QFileInfo(QString::fromStdString(document.video_path())).fileName().toStdString()
				: QFileInfo(QString::fromStdString(path)).fileName().toStdString();
		unsaved_documents_.push_back({next_unsaved_id_++, title, path, {}, std::nullopt});
	}
	emit unsavedDocumentsChanged();

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
		const bool save_ok = store_.finish(session_, config_.values().auto_export.json);
		document_unsaved_ = !save_ok && document_unsaved_;
		if (document_unsaved_)
			warn("Warning: Failed to persist session during shutdown.", 5000);
		perform_auto_export(path);
	}
	timeline_.stop();
	preserve_current_document();
	for (auto &entry : unsaved_documents_) {
		if (!entry.memory)
			continue;
		entry.recovery_path = store_.create_recovery_copy(*entry.memory);
		if (!entry.recovery_path.empty())
			entry.memory.reset();
		else
			warn("Warning: Unsaved markers remain only in memory. All recovery storage failed. "
			     "Save them before closing OBS.",
			     10000);
	}
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
		warn("Not recording! Cannot stamp marker.", 2000);
		return false;
	}
	if (capture_blocked_) {
		warn("Marker capture is suspended. Save an unsaved document first.", 5000);
		return false;
	}

	const auto capture = poll_recording_state();
	if (!timeline_.active() || capture_blocked_)
		return false;

	if (type_index < 0 || type_index >= 4) {
		type_index = 0;
	}

	const auto &cfg = config_.values();
	std::string label = cfg.marker_types[type_index].label;
	std::string color = cfg.marker_types[type_index].color;
	bool paused = is_paused();

	MemoMarker m = session_.add_marker_at_frame(
		timeline_.relative_frames(capture.total_frames), type_index, label, color, comment, paused,
		QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString());
	if (!m.id) {
		warn("Marker ID range exhausted. Save this document before starting a new one.", 5000);
		return false;
	}
	journal_current_ = store_.append({{"op", "add"}, {"marker", SessionCodec::encode_marker(m)}}) &&
			   journal_current_;
	document_unsaved_ = true;
	++document_revision_;
	int row = static_cast<int>(session_.get_markers().size()) - 1;
	record_edit({{static_cast<size_t>(row), std::nullopt, m}});

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
	for (const auto &marker : session_.get_markers())
		if (marker.id == id) {
			auto candidate = marker;
			candidate.type_index = type;
			if (marker.type_index >= 0 && marker.type_index < 4 &&
			    marker.label == config_.values().marker_types[marker.type_index].label)
				candidate.label = config_.values().marker_types[type].label;
			candidate.color = config_.values().marker_types[type].color;
			return apply_marker_update(candidate);
		}
	return false;
}
bool SessionController::update_marker_data(uint32_t id, const std::string &label, const std::string &color,
					   const std::string &comment)
{
	for (const auto &marker : session_.get_markers())
		if (marker.id == id) {
			auto candidate = marker;
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
			const auto previous = markers[row];
			if (!session_.update_marker(candidate.id, candidate.label, candidate.color, candidate.comment,
						    candidate.type_index))
				return false;
			journal_update(candidate.id);
			record_edit({{row, previous, candidate}});
			++document_revision_;
			persist_edits();
			emit markerUpdated(candidate, static_cast<int>(row));
			return true;
		}
	return false;
}

bool SessionController::delete_marker(uint32_t id)
{
	const auto &markers = session_.get_markers();
	for (size_t i = 0; i < markers.size(); ++i) {
		if (markers[i].id == id) {
			const auto previous = markers[i];
			session_.delete_marker(id);
			record_edit({{i, previous, std::nullopt}});
			++document_revision_;
			if (timeline_.active())
				journal_current_ = store_.append({{"op", "delete"}, {"id", static_cast<qint64>(id)}}) &&
						   journal_current_;
			persist_edits();
			emit markerRemoved(id, static_cast<int>(i));
			return true;
		}
	}
	return false;
}

void SessionController::clear_markers()
{
	MarkerEditHistory::Command command;
	const auto &markers = session_.get_markers();
	for (size_t row = 0; row < markers.size(); ++row)
		command.push_back({row, markers[row], std::nullopt});
	if (command.empty())
		return;
	session_.clear_markers();
	record_edit(std::move(command));
	++document_revision_;
	if (timeline_.active())
		journal_current_ = store_.append({{"op", "clear"}}) && journal_current_;
	persist_edits();
	emit markersReset({}, session_.frame_rate());
}

bool SessionController::load_from_json(const std::string &path)
{
	if (is_recording() || store_.journaling())
		return false;
	SessionStore reader;
	RecordingSession document;
	if (reader.load(path, document) && preserve_current_document() && store_.adopt_read(std::move(reader))) {
		session_ = std::move(document);
		history_.clear();
		emit editHistoryChanged();
		journal_current_ = false;
		++document_revision_;
		history_path_ = path;
		recovered_ = false;
		emit markersReset(session_.get_markers(), session_.frame_rate());
		return true;
	}
	return false;
}

bool SessionController::recover_from_cache(const std::string &cache_path)
{
	if (is_recording() || store_.journaling())
		return false;
	SessionStore reader;
	RecordingSession document;
	if (reader.recover(cache_path, document) && preserve_current_document() &&
	    store_.adopt_read(std::move(reader))) {
		session_ = std::move(document);
		history_.clear();
		emit editHistoryChanged();
		journal_current_ = true;
		++document_revision_;
		history_path_ = cache_path;
		recovered_ = true;
		document_unsaved_ = !session_.get_markers().empty();
		emit markersReset(session_.get_markers(), session_.frame_rate());
		return true;
	}
	return false;
}

void SessionController::persist_edits()
{
	document_unsaved_ = true;
	if (timeline_.active()) {
		check_journal();
	} else if (config_.values().auto_export.json) {
		document_unsaved_ = !store_.save(session_);
		if (document_unsaved_)
			warn("Warning: Failed to save edited markers. Use Unsaved to save JSON elsewhere.", 5000);
		// A closed journal cannot contain edits made after STOPPED/recovery.
		journal_current_ = false;
	} else {
		journal_current_ = store_.save_recovery_snapshot(session_);
		if (!journal_current_)
			warn("Could not save recovery data for edited markers. Use Unsaved to save JSON elsewhere.");
	}
	emit unsavedDocumentsChanged();
}

void SessionController::check_journal()
{
	if (!store_.journal_healthy() && !journal_warning_shown_) {
		journal_warning_shown_ = true;
		warn("Warning: Recovery cache could not be written. Export JSON to preserve markers.", 5000);
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
		if (!dir.mkpath(".")) {
			all_success = false;
			return;
		}
		QString path;
		for (int attempt = 0; attempt < 1000; ++attempt) {
			const auto suffix =
				attempt == 0 ? QString()
					     : ".timestamp-memo." + QString::fromStdString(session_.session_id()) +
						       (attempt == 1 ? QString() : "." + QString::number(attempt));
			const auto candidate = dir.filePath(base_name + suffix + "." + QString::fromStdString(ext));
			QFile reservation(candidate);
			// NewOnly also closes the exists-check/create race. Never truncate an existing file.
			if (reservation.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
				path = candidate;
				reservation.close();
				break;
			}
			if (!QFile::exists(candidate))
				break; // Permission/storage failure is not a name collision.
		}
		if (path.isEmpty()) {
			all_success = false;
			return;
		}
		if (!reg.export_by_id(id, session_, path.toStdString())) {
			QFile::remove(path); // Remove only this operation's newly reserved output.
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
		warn("Warning: Some auto-export formats failed to save!", 5000);
	}
	return all_success;
}

void SessionController::check_recording_file_changed()
{
	if (is_recording())
		poll_recording_state();
}
RecordingSnapshot SessionController::poll_recording_state()
{
	bridge_.drain_pending_events();
	const auto capture = bridge_.snapshot();
	if (!is_recording())
		return capture;
	if (!capture_blocked_ && session_.video_path().empty() && !capture.path.empty()) {
		session_.set_video_path(capture.path);
		++document_revision_;
		journal_current_ =
			store_.append({{"op", "video_path"}, {"video_path", QString::fromStdString(capture.path)}}) &&
			journal_current_;
		check_journal();
		emit videoPathResolved(QString::fromStdString(capture.path));
	}
	emit recordTimeChanged(TimecodeHelper::frame_index_to_ms(timeline_.relative_frames(capture.total_frames),
								 session_.frame_rate()));
	return capture;
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

	emit markersReset(session_.get_markers(), session_.frame_rate());
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
			++document_revision_;
			journal_current_ = store_.append({{"op", "video_path"},
							  {"video_path", QString::fromStdString(video_path)}}) &&
					   journal_current_;
		}
	}

	std::string json_path;
	bool save_ok = store_.finish(session_, config_.values().auto_export.json, &json_path);
	document_unsaved_ = !save_ok && document_unsaved_;
	timeline_.stop();
	capture_blocked_ = false;

	bool export_ok = perform_auto_export(video_path);

	emit sessionStopped(QString::fromStdString(json_path));
	emit unsavedDocumentsChanged();

	if (save_ok && export_ok) {
		notify("Recording stopped. Markers saved.", 3000);
	} else {
		warn("Recording stopped. Warning: Some marker files could not be saved. Check the OBS log.", 5000);
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
	document_unsaved_ = !save_ok && document_unsaved_;
	bool export_ok = perform_auto_export(old_video_path);

	const auto capture = bridge_.snapshot();
	const auto new_path = path.toStdString();
	const auto fps = capture.fps;
	const auto width = capture.width, height = capture.height;
	start_document(new_path, fps, width, height);
	journal_warning_shown_ = false;

	emit markersReset(session_.get_markers(), session_.frame_rate());
	emit videoPathResolved(QString::fromStdString(new_path));
	notify("Recording split into: " + new_path, 3000);
	check_journal();
	if (!save_ok || !export_ok)
		warn("Warning: Previous segment could not be saved. Check the recovery cache and OBS log.", 5000);
}

void SessionController::start_document(const std::string &path, const VideoFrameRate &fps, uint32_t width,
				       uint32_t height)
{
	pending_capture_.path = path;
	pending_capture_.fps = fps;
	pending_capture_.width = width;
	pending_capture_.height = height;
	capture_blocked_ = !preserve_current_document();
	if (capture_blocked_)
		return;
	history_path_.clear();
	recovered_ = false;
	history_.clear();
	emit editHistoryChanged();
	session_.replace({path, QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
			  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(), fps, width,
			  height});
	journal_current_ = store_.begin(session_);
	++document_revision_;
	document_unsaved_ = false;
}
void SessionController::journal_update(uint32_t id)
{
	if (!timeline_.active())
		return;
	for (const auto &m : session_.get_markers())
		if (m.id == id) {
			auto op = SessionCodec::encode_marker(m);
			op["op"] = "update";
			journal_current_ = store_.append(op) && journal_current_;
			break;
		}
}

QString SessionController::document_title() const
{
	std::string path = history_path_.empty() ? session_.video_path() : history_path_;
	const auto title = path.empty()
				   ? (timeline_.active() ? "Resolving recording file..."
							 : (session_.session_id().empty() ? "No session loaded"
											  : "Unresolved recording"))
				   : QFileInfo(QString::fromStdString(path)).fileName();
	return title + (history_path_.empty() ? QString() : QString(recovered_ ? " (Recovered)" : " (Loaded)")) +
	       (capture_blocked_ ? " (Unsaved; marker capture suspended)"
				 : (has_unsaved_current() ? " (Unsaved)" : ""));
}

void SessionController::record_edit(MarkerEditHistory::Command command)
{
	if (!history_.record(std::move(command)))
		warn("This edit exceeds the Undo history limit. Earlier Undo history was cleared.");
	emit editHistoryChanged();
}

bool SessionController::undo()
{
	return apply_history(false);
}
bool SessionController::redo()
{
	return apply_history(true);
}

bool SessionController::apply_history(bool forward)
{
	const auto *command = forward ? history_.redo_command() : history_.undo_command();
	if (!command)
		return false;
	auto next = session_; // Transactional: an invalid command cannot partially change the document.
	for (const auto &change : *command) {
		const auto &from = forward ? change.before : change.after;
		const auto &to = forward ? change.after : change.before;
		const bool applied =
			from && to ? next.update_marker(to->id, to->label, to->color, to->comment, to->type_index)
				   : (to ? next.insert_marker(change.row, *to) : next.delete_marker(from->id));
		if (!applied) {
			warn("The edit could not be restored. The current document was retained.");
			return false;
		}
	}
	session_ = std::move(next);
	if (timeline_.active()) {
		for (const auto &change : *command) {
			const auto &from = forward ? change.before : change.after;
			const auto &to = forward ? change.after : change.before;
			if (from && to)
				journal_update(to->id);
			else if (to)
				journal_current_ = store_.append({{"op", "add"},
								  {"marker", SessionCodec::encode_marker(*to)},
								  {"row", static_cast<qint64>(change.row)}}) &&
						   journal_current_;
			else
				journal_current_ =
					store_.append({{"op", "delete"}, {"id", static_cast<qint64>(from->id)}}) &&
					journal_current_;
		}
	}
	if (forward)
		history_.did_redo();
	else
		history_.did_undo();
	++document_revision_;
	persist_edits();
	emit markersReset(session_.get_markers(), session_.frame_rate());
	emit editHistoryChanged();
	return true;
}

bool SessionController::discard_unsaved_document(uint64_t id)
{
	if (id == 0) {
		if (!has_unsaved_current() || !store_.discard_cache())
			return false;
		document_unsaved_ = false;
		journal_current_ = false;
		session_.clear_markers();
		history_.clear();
		++document_revision_;
		resume_capture();
		emit markersReset(session_.get_markers(), session_.frame_rate());
		emit editHistoryChanged();
	} else {
		auto entry = std::find_if(unsaved_documents_.begin(), unsaved_documents_.end(),
					  [id](const auto &item) { return item.id == id; });
		if (entry == unsaved_documents_.end())
			return false;
		for (const auto &path : {entry->recovery_path, entry->journal_path})
			if (!path.empty() && QFile::exists(QString::fromStdString(path)) &&
			    !QFile::remove(QString::fromStdString(path))) {
				warn("Could not remove all recovery files. The unsaved entry was retained.");
				return false;
			}
		unsaved_documents_.erase(entry);
		resume_capture();
	}
	emit unsavedDocumentsChanged();
	return true;
}
