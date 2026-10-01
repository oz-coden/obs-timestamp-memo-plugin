#include "recording-session.hpp"
#include "plugin-config.hpp"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <obs-module.h>
#include <QUuid>
#include <unordered_set>

RecordingSession::RecordingSession() {}

RecordingSession::RecordingSession(const RecordingSession &other)
{
	std::lock_guard<std::recursive_mutex> lock(other.mutex_);
	video_path_ = other.video_path_;
	session_id_ = other.session_id_;
	started_at_ = other.started_at_;
	fps_ = other.fps_;
	width_ = other.width_;
	height_ = other.height_;
	markers_ = other.markers_;
	next_marker_id_ = other.next_marker_id_;
}

RecordingSession::~RecordingSession()
{
	if (cache_stream_) {
		cache_stream_->flush();
	}
	if (cache_file_ && cache_file_->isOpen()) {
		cache_file_->close();
	}
}

bool RecordingSession::start_session(const std::string &video_path, const VideoFrameRate &fps, uint32_t width,
				     uint32_t height)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (active_)
		return false;

	if (cache_stream_) {
		cache_stream_->flush();
	}
	if (cache_file_ && cache_file_->isOpen()) {
		cache_file_->close();
	}
	cache_stream_.reset();
	cache_file_.reset();

	active_ = true;
	video_path_ = video_path;
	fps_ = fps;
	if (fps_.num == 0)
		fps_.num = 60;
	if (fps_.den == 0)
		fps_.den = 1;
	width_ = (width == 0) ? 1920 : width;
	height_ = (height == 0) ? 1080 : height;
	session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
	started_at_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
	markers_.clear();
	next_marker_id_ = 1;
	source_json_path_.clear();
	cache_file_path_.clear();
	journal_healthy_ = open_journal();
	return journal_healthy_;
}

bool RecordingSession::open_journal()
{
	cache_file_path_ = get_cache_file_path();
	if (!cache_file_path_.empty()) {
		QFileInfo cfi(QString::fromStdString(cache_file_path_));
		cfi.dir().mkpath(".");

		auto file = std::make_unique<QFile>(QString::fromStdString(cache_file_path_));
		if (file->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::NewOnly)) {
			cache_file_ = std::move(file);
			cache_stream_ = std::make_unique<QTextStream>(cache_file_.get());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			cache_stream_->setCodec("UTF-8");
#endif

			QJsonObject header;
			header["op"] = "header";
			header["session_id"] = QString::fromStdString(session_id_);
			header["video_path"] = QString::fromStdString(video_path_);
			header["started_at"] = QString::fromStdString(started_at_);
			header["fps_num"] = static_cast<qint64>(fps_.num);
			header["fps_den"] = static_cast<qint64>(fps_.den);
			header["width"] = static_cast<qint64>(width_);
			header["height"] = static_cast<qint64>(height_);

			return flush_journal_entry(header);
		}
	}

	return false;
}

bool RecordingSession::stop_session(std::string *out_json_path, bool save_json)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	if (!active_) {
		return false;
	}

	active_ = false;

	if (cache_stream_) {
		cache_stream_->flush();
	}
	if (cache_file_ && cache_file_->isOpen()) {
		cache_file_->close();
	}
	cache_stream_.reset();
	cache_file_.reset();

	std::string json_path = "";
	bool save_ok = false;

	if (save_json && !video_path_.empty()) {
		QFileInfo fi(QString::fromStdString(video_path_));
		fi.dir().mkpath(".");
		QString target = fi.dir().filePath(fi.completeBaseName() + ".json");
		json_path = target.toStdString();
		save_ok = save_to_json(json_path);
	}

	if (save_ok) {
		cleanup_cache_file(true);
	}

	if (out_json_path) {
		*out_json_path = save_ok ? json_path : "";
	}

	return save_json ? save_ok : journal_healthy_;
}

std::string RecordingSession::get_cache_file_path() const
{
	QFileInfo fi(QString::fromStdString(video_path_));
	QDir dir = fi.dir();
	if (video_path_.empty()) {
		char *config_path = obs_module_config_path("cache");
		if (!config_path)
			return "";
		dir = QDir(QString::fromUtf8(config_path));
		bfree(config_path);
	}
	QString stem = video_path_.empty() ? "recording" : fi.completeBaseName();
	return dir.filePath(stem + "." + QString::fromStdString(session_id_) + ".tmp.jsonl").toStdString();
}

bool RecordingSession::flush_journal_entry(const QJsonObject &entry)
{
	if (!cache_stream_ || !cache_file_) {
		journal_healthy_ = false;
		return false;
	}

	QJsonDocument doc(entry);
	*cache_stream_ << QString::fromUtf8(doc.toJson(QJsonDocument::Compact)) << "\n";
	cache_stream_->flush();
	journal_healthy_ = cache_stream_->status() == QTextStream::Ok && cache_file_->flush();
	return journal_healthy_;
}

bool RecordingSession::journal_healthy() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return journal_healthy_;
}

void RecordingSession::cleanup_cache_file(bool force)
{
	if (!force && PluginConfig::instance().keep_tmp_cache_on_crash) {
		return;
	}

	if (!cache_file_path_.empty()) {
		QFile::remove(QString::fromStdString(cache_file_path_));
		cache_file_path_.clear();
	}
}

MemoMarker RecordingSession::add_marker(uint64_t ms, int type_index, const std::string &label, const std::string &color,
					const std::string &comment, bool is_paused)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	MemoMarker marker =
		MemoMarker::create(next_marker_id_++, ms, fps_, type_index, label, color, comment, is_paused);
	markers_.push_back(marker);

	QJsonObject op;
	op["op"] = "add";
	op["marker"] = marker.to_json();
	flush_journal_entry(op);

	return marker;
}

bool RecordingSession::update_marker(uint32_t marker_id, const std::string &label, const std::string &color,
				     const std::string &comment, int type_index)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	for (auto &m : markers_) {
		if (m.id == marker_id) {
			m.label = label;
			m.color = color;
			m.comment = comment;
			if (type_index >= 0 && type_index < 4)
				m.type_index = type_index;

			QJsonObject op;
			op["op"] = "update";
			op["id"] = static_cast<qint64>(marker_id);
			op["label"] = QString::fromStdString(label);
			op["color"] = QString::fromStdString(color);
			op["comment"] = QString::fromStdString(comment);
			op["type_index"] = m.type_index;
			flush_journal_entry(op);

			return true;
		}
	}
	return false;
}

bool RecordingSession::delete_marker(uint32_t marker_id)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	for (auto it = markers_.begin(); it != markers_.end(); ++it) {
		if (it->id == marker_id) {
			markers_.erase(it);

			QJsonObject op;
			op["op"] = "delete";
			op["id"] = static_cast<qint64>(marker_id);
			flush_journal_entry(op);

			return true;
		}
	}
	return false;
}

void RecordingSession::clear_markers()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	markers_.clear();

	QJsonObject op;
	op["op"] = "clear";
	flush_journal_entry(op);
}

bool RecordingSession::is_active() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return active_;
}

std::string RecordingSession::video_path() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return video_path_;
}

void RecordingSession::set_video_path(const std::string &path)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (path.empty() || path == video_path_)
		return;

	video_path_ = path;
	if (active_) {
		QJsonObject op;
		op["op"] = "video_path";
		op["video_path"] = QString::fromStdString(path);
		flush_journal_entry(op);
	}
}

std::string RecordingSession::session_id() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return session_id_;
}

std::string RecordingSession::started_at() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return started_at_;
}

VideoFrameRate RecordingSession::frame_rate() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return fps_;
}

uint32_t RecordingSession::width() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return width_;
}

uint32_t RecordingSession::height() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return height_;
}

std::vector<MemoMarker> RecordingSession::get_markers() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	return markers_;
}

QJsonObject RecordingSession::to_json() const
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	QJsonObject root;
	root["schema_version"] = "1.0.0";
	root["session_id"] = QString::fromStdString(session_id_);
	root["video_file_path"] = QString::fromStdString(video_path_);

	QFileInfo fi(QString::fromStdString(video_path_));
	root["video_file_name"] = fi.fileName();
	root["started_at_utc"] = QString::fromStdString(started_at_);

	QJsonObject video_info;
	video_info["fps_num"] = static_cast<qint64>(fps_.num);
	video_info["fps_den"] = static_cast<qint64>(fps_.den);
	video_info["fps"] = fps_.fps();
	video_info["is_drop_frame"] = fps_.is_drop_frame();
	video_info["width"] = static_cast<qint64>(width_);
	video_info["height"] = static_cast<qint64>(height_);
	root["video_info"] = video_info;

	QJsonArray markers_array;
	for (const auto &m : markers_) {
		markers_array.append(m.to_json());
	}
	root["markers"] = markers_array;

	return root;
}

bool RecordingSession::save_to_json(const std::string &target_json_path)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	std::string path = target_json_path;
	if (path.empty())
		path = source_json_path_;
	if (path.empty()) {
		if (video_path_.empty()) {
			return false;
		}
		QFileInfo fi(QString::fromStdString(video_path_));
		path = fi.dir().filePath(fi.completeBaseName() + ".json").toStdString();
	}

	QFileInfo fi(QString::fromStdString(path));
	fi.dir().mkpath(".");

	QJsonObject root = to_json();
	QJsonDocument doc(root);

	QSaveFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	QByteArray data = doc.toJson(QJsonDocument::Indented);
	if (file.write(data) != data.size()) {
		file.cancelWriting();
		return false;
	}

	if (!file.commit())
		return false;
	if (!active_) {
		source_json_path_ = path;
		cleanup_cache_file(true);
	}
	return true;
}

bool RecordingSession::load_from_json(const std::string &json_path)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	if (active_) {
		return false;
	}

	QFile file(QString::fromStdString(json_path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}

	QByteArray data = file.readAll();
	file.close();

	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(data, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		return false;
	}

	QJsonObject root = doc.object();
	if (!root["session_id"].isString() || root["session_id"].toString().isEmpty() ||
	    !root["video_info"].isObject() || !root["markers"].isArray())
		return false;
	for (const auto &value : root["markers"].toArray()) {
		if (!value.isObject())
			return false;
	}
	cache_stream_.reset();
	cache_file_.reset();
	cache_file_path_.clear();
	source_json_path_ = json_path;
	session_id_ = root["session_id"].toString().toStdString();
	video_path_ = root["video_file_path"].toString().toStdString();
	started_at_ = root["started_at_utc"].toString().toStdString();

	fps_ = VideoFrameRate{60, 1};
	width_ = 1920;
	height_ = 1080;

	if (root.contains("video_info") && root["video_info"].isObject()) {
		QJsonObject vinfo = root["video_info"].toObject();
		qint64 num = vinfo["fps_num"].toInteger(60);
		qint64 den = vinfo["fps_den"].toInteger(1);
		qint64 w = vinfo["width"].toInteger(1920);
		qint64 h = vinfo["height"].toInteger(1080);

		if (num > 0 && num <= 1000000 && den > 0 && den <= 1000000 &&
		    static_cast<double>(num) / static_cast<double>(den) >= 1.0 &&
		    static_cast<double>(num) / static_cast<double>(den) <= 1000.0)
			fps_ = VideoFrameRate{static_cast<uint32_t>(num), static_cast<uint32_t>(den)};
		width_ = (w > 0 && w <= 32768) ? static_cast<uint32_t>(w) : 1920;
		height_ = (h > 0 && h <= 32768) ? static_cast<uint32_t>(h) : 1080;
	}

	markers_.clear();
	uint32_t max_id = 0;
	std::unordered_set<uint32_t> seen_ids;

	if (root.contains("markers") && root["markers"].isArray()) {
		QJsonArray arr = root["markers"].toArray();
		for (const auto val : arr) {
			if (val.isObject()) {
				MemoMarker m = MemoMarker::from_json(val.toObject());
				if (m.id == 0 || seen_ids.find(m.id) != seen_ids.end()) {
					m.id = max_id + 1;
				}
				seen_ids.insert(m.id);
				m.timecode_ndf = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps_, true);
				m.timecode_df = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps_);
				markers_.push_back(m);
				if (m.id > max_id) {
					max_id = m.id;
				}
			}
		}
	}
	next_marker_id_ = max_id + 1;
	active_ = false;

	return true;
}

bool RecordingSession::recover_from_cache(const std::string &cache_path, RecordingSession &out_session)
{
	QFile file(QString::fromStdString(cache_path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}

	RecordingSession temp;
	bool has_header = false;
	std::unordered_set<uint32_t> seen_ids;
	temp.fps_ = VideoFrameRate{60, 1};
	temp.width_ = 1920;
	temp.height_ = 1080;

	while (!file.atEnd()) {
		QByteArray raw_line = file.readLine();
		QByteArray line = raw_line.trimmed();
		if (line.isEmpty())
			continue;

		QJsonDocument doc = QJsonDocument::fromJson(line);
		if (!doc.isObject()) {
			// A crash can leave only the final append incomplete. Corruption
			// in a completed record must not silently change the recovered state.
			if (file.atEnd() && !raw_line.endsWith('\n'))
				break;
			return false;
		}

		QJsonObject obj = doc.object();
		QString op = obj["op"].toString();
		if (op.isEmpty()) {
			op = obj["type"].toString();
		}
		if (!has_header && op != "header")
			return false;

		if (op == "header") {
			if (has_header)
				return false;
			has_header = true;
			temp.session_id_ = obj["session_id"].toString().toStdString();
			temp.video_path_ = obj["video_path"].toString().toStdString();
			temp.started_at_ = obj["started_at"].toString().toStdString();
			qint64 num = obj["fps_num"].toInteger(60);
			qint64 den = obj["fps_den"].toInteger(1);
			if (num > 0 && num <= 1000000 && den > 0 && den <= 1000000 &&
			    static_cast<double>(num) / static_cast<double>(den) >= 1.0 &&
			    static_cast<double>(num) / static_cast<double>(den) <= 1000.0)
				temp.fps_ = VideoFrameRate{static_cast<uint32_t>(num), static_cast<uint32_t>(den)};
			qint64 w = obj["width"].toInteger(1920);
			qint64 h = obj["height"].toInteger(1080);
			temp.width_ = (w > 0 && w <= 32768) ? static_cast<uint32_t>(w) : 1920;
			temp.height_ = (h > 0 && h <= 32768) ? static_cast<uint32_t>(h) : 1080;
		} else if (op == "add" || op == "marker") {
			QJsonObject mobj = obj.contains("marker") ? obj["marker"].toObject() : obj;
			MemoMarker m = MemoMarker::from_json(mobj);
			if (m.id == 0 || !seen_ids.insert(m.id).second)
				return false;
			temp.markers_.push_back(m);
			if (m.id >= temp.next_marker_id_) {
				temp.next_marker_id_ = m.id + 1;
			}
		} else if (op == "update") {
			uint32_t uid = static_cast<uint32_t>(obj["id"].toInteger(0));
			std::string ulabel = obj["label"].toString().toStdString();
			std::string ucolor = obj["color"].toString().toStdString();
			std::string ucomment = obj["comment"].toString().toStdString();
			for (auto &m : temp.markers_) {
				if (m.id == uid) {
					m.label = ulabel;
					m.color = ucolor;
					m.comment = ucomment;
					if (obj.contains("type_index"))
						m.type_index = qBound(0, obj["type_index"].toInt(), 3);
					break;
				}
			}
		} else if (op == "delete") {
			uint32_t did = static_cast<uint32_t>(obj["id"].toInteger(0));
			for (auto it = temp.markers_.begin(); it != temp.markers_.end(); ++it) {
				if (it->id == did) {
					temp.markers_.erase(it);
					seen_ids.erase(did);
					break;
				}
			}
		} else if (op == "clear") {
			temp.markers_.clear();
			seen_ids.clear();
			temp.next_marker_id_ = 1;
		} else if (op == "video_path") {
			temp.video_path_ = obj["video_path"].toString().toStdString();
		}
	}

	file.close();
	if (file.error() != QFileDevice::NoError)
		return false;

	if (!has_header || temp.session_id_.empty()) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> lock(out_session.mutex_);
	if (out_session.active_)
		return false;
	out_session.cache_stream_.reset();
	out_session.cache_file_.reset();
	out_session.cache_file_path_ = cache_path;
	out_session.source_json_path_.clear();
	out_session.session_id_ = temp.session_id_;
	out_session.video_path_ = temp.video_path_;
	out_session.started_at_ = temp.started_at_;
	out_session.fps_ = temp.fps_;
	out_session.width_ = temp.width_;
	out_session.height_ = temp.height_;
	out_session.markers_ = temp.markers_;
	for (auto &marker : out_session.markers_) {
		marker.timecode_ndf = TimecodeHelper::frame_index_to_smpte(marker.frame_index, temp.fps_, true);
		marker.timecode_df = TimecodeHelper::frame_index_to_smpte(marker.frame_index, temp.fps_);
	}
	out_session.next_marker_id_ = temp.next_marker_id_;
	out_session.active_ = false;

	return true;
}
