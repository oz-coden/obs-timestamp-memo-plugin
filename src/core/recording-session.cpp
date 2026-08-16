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
#include <QUuid>
#include <unordered_set>

RecordingSession::RecordingSession() {}

RecordingSession::~RecordingSession()
{
	if (cache_stream_.is_open()) {
		cache_stream_.flush();
		cache_stream_.close();
	}
}

bool RecordingSession::start_session(const std::string &video_path, const VideoFrameRate &fps, uint32_t width,
				     uint32_t height)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (cache_stream_.is_open()) {
		cache_stream_.flush();
		cache_stream_.close();
	}

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

	cache_file_path_ = get_cache_file_path();
	if (!cache_file_path_.empty()) {
		QFileInfo cfi(QString::fromStdString(cache_file_path_));
		cfi.dir().mkpath(".");

		cache_stream_.open(cache_file_path_, std::ios::out | std::ios::trunc);
		if (cache_stream_.is_open()) {
			QJsonObject header;
			header["op"] = "header";
			header["session_id"] = QString::fromStdString(session_id_);
			header["video_path"] = QString::fromStdString(video_path_);
			header["started_at"] = QString::fromStdString(started_at_);
			header["fps_num"] = static_cast<qint64>(fps_.num);
			header["fps_den"] = static_cast<qint64>(fps_.den);
			header["width"] = static_cast<qint64>(width_);
			header["height"] = static_cast<qint64>(height_);

			QJsonDocument doc(header);
			cache_stream_ << doc.toJson(QJsonDocument::Compact).toStdString() << "\n";
			cache_stream_.flush();
		}
	}

	return true;
}

bool RecordingSession::stop_session(std::string *out_json_path)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (!active_) {
		return false;
	}

	active_ = false;

	if (cache_stream_.is_open()) {
		cache_stream_.flush();
		cache_stream_.close();
	}

	std::string json_path = "";
	bool save_ok = false;

	if (!video_path_.empty()) {
		QFileInfo fi(QString::fromStdString(video_path_));
		fi.dir().mkpath(".");
		QString target = fi.dir().filePath(fi.completeBaseName() + ".json");
		json_path = target.toStdString();
		save_ok = save_to_json(json_path);
	}

	if (save_ok) {
		cleanup_cache_file();
	}

	if (out_json_path) {
		*out_json_path = json_path;
	}

	return save_ok;
}

std::string RecordingSession::get_cache_file_path() const
{
	if (video_path_.empty()) {
		return "";
	}
	QFileInfo fi(QString::fromStdString(video_path_));
	return fi.dir().filePath(fi.completeBaseName() + ".tmp.jsonl").toStdString();
}

void RecordingSession::flush_journal_entry(const QJsonObject &entry)
{
	if (!cache_stream_.is_open()) {
		return;
	}

	QJsonDocument doc(entry);
	cache_stream_ << doc.toJson(QJsonDocument::Compact).toStdString() << "\n";
	cache_stream_.flush();
}

void RecordingSession::cleanup_cache_file()
{
	if (!cache_file_path_.empty()) {
		QFile::remove(QString::fromStdString(cache_file_path_));
		cache_file_path_.clear();
	}
}

MemoMarker RecordingSession::add_marker(uint64_t ms, int type_index, const std::string &label, const std::string &color,
					const std::string &comment, bool is_paused)
{
	std::lock_guard<std::mutex> lock(mutex_);

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
				     const std::string &comment)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto &m : markers_) {
		if (m.id == marker_id) {
			m.label = label;
			m.color = color;
			m.comment = comment;

			QJsonObject op;
			op["op"] = "update";
			op["id"] = static_cast<qint64>(marker_id);
			op["label"] = QString::fromStdString(label);
			op["color"] = QString::fromStdString(color);
			op["comment"] = QString::fromStdString(comment);
			flush_journal_entry(op);

			return true;
		}
	}
	return false;
}

bool RecordingSession::delete_marker(uint32_t marker_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
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
	std::lock_guard<std::mutex> lock(mutex_);
	markers_.clear();
	next_marker_id_ = 1;

	QJsonObject op;
	op["op"] = "clear";
	flush_journal_entry(op);
}

std::vector<MemoMarker> RecordingSession::get_markers() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return markers_;
}

QJsonObject RecordingSession::to_json() const
{
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
	std::string path = target_json_path;
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

	return file.commit();
}

bool RecordingSession::load_from_json(const std::string &json_path)
{
	std::lock_guard<std::mutex> lock(mutex_);

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
	session_id_ = root["session_id"].toString().toStdString();
	video_path_ = root["video_file_path"].toString().toStdString();
	started_at_ = root["started_at_utc"].toString().toStdString();

	if (root.contains("video_info") && root["video_info"].isObject()) {
		QJsonObject vinfo = root["video_info"].toObject();
		qint64 num = vinfo["fps_num"].toInteger(60);
		qint64 den = vinfo["fps_den"].toInteger(1);
		qint64 w = vinfo["width"].toInteger(1920);
		qint64 h = vinfo["height"].toInteger(1080);

		fps_.num = (num > 0) ? static_cast<uint32_t>(num) : 60;
		fps_.den = (den > 0) ? static_cast<uint32_t>(den) : 1;
		width_ = (w > 0) ? static_cast<uint32_t>(w) : 1920;
		height_ = (h > 0) ? static_cast<uint32_t>(h) : 1080;
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

	out_session.clear_markers();

	while (!file.atEnd()) {
		QByteArray line = file.readLine().trimmed();
		if (line.isEmpty())
			continue;

		QJsonDocument doc = QJsonDocument::fromJson(line);
		if (!doc.isObject())
			continue;

		QJsonObject obj = doc.object();
		QString op = obj["op"].toString();
		if (op.isEmpty()) {
			op = obj["type"].toString();
		}

		if (op == "header") {
			out_session.session_id_ = obj["session_id"].toString().toStdString();
			out_session.video_path_ = obj["video_path"].toString().toStdString();
			out_session.started_at_ = obj["started_at"].toString().toStdString();
			qint64 num = obj["fps_num"].toInteger(60);
			qint64 den = obj["fps_den"].toInteger(1);
			out_session.fps_.num = (num > 0) ? static_cast<uint32_t>(num) : 60;
			out_session.fps_.den = (den > 0) ? static_cast<uint32_t>(den) : 1;
			qint64 w = obj["width"].toInteger(1920);
			qint64 h = obj["height"].toInteger(1080);
			out_session.width_ = (w > 0) ? static_cast<uint32_t>(w) : 1920;
			out_session.height_ = (h > 0) ? static_cast<uint32_t>(h) : 1080;
		} else if (op == "add" || op == "marker") {
			QJsonObject mobj = obj.contains("marker") ? obj["marker"].toObject() : obj;
			MemoMarker m = MemoMarker::from_json(mobj);
			out_session.markers_.push_back(m);
			if (m.id >= out_session.next_marker_id_) {
				out_session.next_marker_id_ = m.id + 1;
			}
		} else if (op == "update") {
			uint32_t uid = static_cast<uint32_t>(obj["id"].toInteger(0));
			std::string ulabel = obj["label"].toString().toStdString();
			std::string ucolor = obj["color"].toString().toStdString();
			std::string ucomment = obj["comment"].toString().toStdString();
			for (auto &m : out_session.markers_) {
				if (m.id == uid) {
					m.label = ulabel;
					m.color = ucolor;
					m.comment = ucomment;
					break;
				}
			}
		} else if (op == "delete") {
			uint32_t did = static_cast<uint32_t>(obj["id"].toInteger(0));
			for (auto it = out_session.markers_.begin(); it != out_session.markers_.end(); ++it) {
				if (it->id == did) {
					out_session.markers_.erase(it);
					break;
				}
			}
		} else if (op == "clear") {
			out_session.markers_.clear();
			out_session.next_marker_id_ = 1;
		}
	}

	file.close();
	return !out_session.markers_.empty() || !out_session.video_path_.empty();
}
