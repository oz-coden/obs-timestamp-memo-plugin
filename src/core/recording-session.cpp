#include "recording-session.hpp"
#include "plugin-config.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

RecordingSession::RecordingSession()
{
}

RecordingSession::~RecordingSession()
{
	if (cache_stream_.is_open()) {
		cache_stream_.close();
	}
}

bool RecordingSession::start_session(const std::string &video_path, const VideoFrameRate &fps, uint32_t width, uint32_t height)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (cache_stream_.is_open()) {
		cache_stream_.close();
	}

	active_ = true;
	video_path_ = video_path;
	fps_ = fps;
	width_ = width;
	height_ = height;
	session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
	started_at_ = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
	markers_.clear();
	next_marker_id_ = 1;

	// キャッシュファイル作成 ({video_path}.tmp.jsonl)
	cache_file_path_ = get_cache_file_path();
	cache_stream_.open(cache_file_path_, std::ios::out | std::ios::trunc);
	if (cache_stream_.is_open()) {
		// ヘッダー行出力
		QJsonObject header;
		header["type"] = "header";
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
		cache_stream_.close();
	}

	// 正式な JSON を書き出す
	std::string json_path = "";
	if (!video_path_.empty()) {
		QFileInfo fi(QString::fromStdString(video_path_));
		QString target = fi.dir().filePath(fi.completeBaseName() + ".json");
		json_path = target.toStdString();
		save_to_json(json_path);
	}

	if (out_json_path) {
		*out_json_path = json_path;
	}

	// キャッシュファイルをクリーンアップ
	cleanup_cache_file();

	return true;
}

std::string RecordingSession::get_cache_file_path() const
{
	if (video_path_.empty()) {
		return "";
	}
	QFileInfo fi(QString::fromStdString(video_path_));
	return fi.dir().filePath(fi.completeBaseName() + ".tmp.jsonl").toStdString();
}

void RecordingSession::flush_marker_to_cache(const MemoMarker &marker)
{
	if (!cache_stream_.is_open()) {
		return;
	}

	QJsonObject mobj = marker.to_json();
	mobj["type"] = "marker";
	QJsonDocument doc(mobj);
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

MemoMarker RecordingSession::add_marker(uint64_t ms, int type_index, const std::string &label,
					const std::string &color, const std::string &comment, bool is_paused)
{
	std::lock_guard<std::mutex> lock(mutex_);

	MemoMarker marker = MemoMarker::create(next_marker_id_++, ms, fps_, type_index, label, color, comment, is_paused);
	markers_.push_back(marker);

	flush_marker_to_cache(marker);

	return marker;
}

bool RecordingSession::update_marker(uint32_t marker_id, const std::string &label, const std::string &color, const std::string &comment)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto &m : markers_) {
		if (m.id == marker_id) {
			m.label = label;
			m.color = color;
			m.comment = comment;
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

	QJsonObject root = to_json();
	QJsonDocument doc(root);

	QFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	file.write(doc.toJson(QJsonDocument::Indented));
	file.close();
	return true;
}

bool RecordingSession::load_from_json(const std::string &json_path)
{
	std::lock_guard<std::mutex> lock(mutex_);

	QFile file(QString::fromStdString(json_path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}

	QByteArray data = file.readAll();
	file.close();

	QJsonDocument doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		return false;
	}

	QJsonObject root = doc.object();
	session_id_ = root["session_id"].toString().toStdString();
	video_path_ = root["video_file_path"].toString().toStdString();
	started_at_ = root["started_at_utc"].toString().toStdString();

	if (root.contains("video_info") && root["video_info"].isObject()) {
		QJsonObject vinfo = root["video_info"].toObject();
		fps_.num = static_cast<uint32_t>(vinfo["fps_num"].toInteger(60));
		fps_.den = static_cast<uint32_t>(vinfo["fps_den"].toInteger(1));
		width_ = static_cast<uint32_t>(vinfo["width"].toInteger(1920));
		height_ = static_cast<uint32_t>(vinfo["height"].toInteger(1080));
	}

	markers_.clear();
	uint32_t max_id = 0;
	if (root.contains("markers") && root["markers"].isArray()) {
		QJsonArray arr = root["markers"].toArray();
		for (const auto &val : arr) {
			if (val.isObject()) {
				MemoMarker m = MemoMarker::from_json(val.toObject());
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
		QString type = obj["type"].toString();

		if (type == "header") {
			out_session.session_id_ = obj["session_id"].toString().toStdString();
			out_session.video_path_ = obj["video_path"].toString().toStdString();
			out_session.started_at_ = obj["started_at"].toString().toStdString();
			out_session.fps_.num = static_cast<uint32_t>(obj["fps_num"].toInteger(60));
			out_session.fps_.den = static_cast<uint32_t>(obj["fps_den"].toInteger(1));
			out_session.width_ = static_cast<uint32_t>(obj["width"].toInteger(1920));
			out_session.height_ = static_cast<uint32_t>(obj["height"].toInteger(1080));
		} else if (type == "marker") {
			MemoMarker m = MemoMarker::from_json(obj);
			out_session.markers_.push_back(m);
			if (m.id >= out_session.next_marker_id_) {
				out_session.next_marker_id_ = m.id + 1;
			}
		}
	}

	file.close();
	return !out_session.markers_.empty();
}
