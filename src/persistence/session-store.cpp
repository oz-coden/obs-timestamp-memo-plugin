#include "session-store.hpp"
#include "session-codec.hpp"
#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <unordered_set>
#include <QUuid>
std::string SessionStore::create_recovery_copy(const RecordingSession &session) const
{
	if (cache_directory_.empty())
		return {};
	QDir dir(QString::fromStdString(cache_directory_) + "/unsaved");
	if (!dir.mkpath("."))
		return {};
	const auto path = dir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json").toStdString();
	SessionStore backup;
	return backup.save(session, path) ? path : std::string();
}
void SessionStore::acknowledge_saved(const std::string &path)
{
	if (journaling())
		return;
	source_path_ = path;
	remove_cache();
}
std::vector<std::string> SessionStore::recovery_copies() const
{
	std::vector<std::string> result;
	if (cache_directory_.empty())
		return result;
	QDir dir(QString::fromStdString(cache_directory_) + "/unsaved");
	for (const auto &file : dir.entryList({"*.json"}, QDir::Files))
		result.push_back(dir.filePath(file).toStdString());
	return result;
}
void SessionStore::close()
{
	if (stream_)
		stream_->flush();
	if (file_)
		file_->close();
	stream_.reset();
	file_.reset();
}
void SessionStore::remove_cache()
{
	if (!cache_path_.empty() && QFile::remove(QString::fromStdString(cache_path_)))
		cache_path_.clear();
}
bool SessionStore::begin(const RecordingSession &session)
{
	close();
	source_path_.clear();
	QFileInfo video(QString::fromStdString(session.video_path()));
	QDir dir = session.video_path().empty() ? QDir(QString::fromStdString(cache_directory_)) : video.dir();
	cache_path_.clear();
	healthy_ = false;
	if (session.video_path().empty() && cache_directory_.empty())
		return false;
	if (!dir.mkpath("."))
		return false;
	QString stem = session.video_path().empty() ? "recording" : video.completeBaseName();
	cache_path_ =
		dir.filePath(stem + "." + QString::fromStdString(session.session_id()) + ".tmp.jsonl").toStdString();
	auto file = std::make_unique<QFile>(QString::fromStdString(cache_path_));
	if (!file->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::NewOnly))
		return false;
	file_ = std::move(file);
	stream_ = std::make_unique<QTextStream>(file_.get());
	auto header = SessionCodec::encode(session)["video_info"].toObject();
	header["op"] = "header";
	header["session_id"] = QString::fromStdString(session.session_id());
	header["video_path"] = QString::fromStdString(session.video_path());
	header["started_at"] = QString::fromStdString(session.started_at());
	return append(header);
}
bool SessionStore::append(const QJsonObject &op)
{
	if (!stream_ || !file_)
		return healthy_ = false;
	*stream_ << QString::fromUtf8(QJsonDocument(op).toJson(QJsonDocument::Compact)) << "\n";
	stream_->flush();
	healthy_ = stream_->status() == QTextStream::Ok && file_->flush();
	return healthy_;
}
bool SessionStore::finish(const RecordingSession &session, bool save_json, std::string *path)
{
	close();
	if (path)
		path->clear();
	if (!save_json)
		return healthy_;
	if (!save(session))
		return false;
	if (path)
		*path = source_path_;
	return true;
}
bool SessionStore::save(const RecordingSession &session, const std::string &target)
{
	std::string path = target.empty() ? source_path_ : target;
	if (path.empty()) {
		if (session.video_path().empty())
			return false;
		QFileInfo video(QString::fromStdString(session.video_path()));
		path = video.dir().filePath(video.completeBaseName() + ".json").toStdString();
	}
	QFileInfo info(QString::fromStdString(path));
	if (!info.dir().mkpath("."))
		return false;
	QSaveFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;
	auto bytes = QJsonDocument(SessionCodec::encode(session)).toJson(QJsonDocument::Indented);
	if (file.write(bytes) != bytes.size()) {
		file.cancelWriting();
		return false;
	}
	if (!file.commit())
		return false;
	if (!journaling()) {
		source_path_ = path;
		remove_cache();
	}
	return true;
}
bool SessionStore::load(const std::string &path, RecordingSession &session)
{
	if (journaling())
		return false;
	QFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::ReadOnly))
		return false;
	auto bytes = file.readAll();
	if (file.error() != QFileDevice::NoError)
		return false;
	auto doc = QJsonDocument::fromJson(bytes);
	if (!doc.isObject() || !SessionCodec::decode(doc.object(), session))
		return false;
	cache_path_.clear();
	source_path_ = path;
	return true;
}
bool SessionStore::recover(const std::string &path, RecordingSession &session)
{
	if (journaling())
		return false;
	QFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;
	QJsonObject root;
	std::vector<MemoMarker> markers;
	std::unordered_set<uint32_t> ids;
	bool header = false;
	while (!file.atEnd()) {
		auto raw = file.readLine(), line = raw.trimmed();
		if (line.isEmpty())
			continue;
		auto doc = QJsonDocument::fromJson(line);
		if (!doc.isObject()) {
			if (file.atEnd() && !raw.endsWith('\n'))
				break;
			return false;
		}
		auto obj = doc.object();
		auto op = obj["op"].toString(obj["type"].toString());
		if (!header && op != "header")
			return false;
		if (op == "header") {
			if (header)
				return false;
			header = true;
			root["session_id"] = obj["session_id"];
			root["video_file_path"] = obj["video_path"];
			root["started_at_utc"] = obj["started_at"];
			root["video_info"] = obj;
		} else if (op == "add" || op == "marker") {
			auto m = SessionCodec::decode_marker(obj.contains("marker") ? obj["marker"].toObject() : obj);
			if (!m.id || !ids.insert(m.id).second)
				return false;
			markers.push_back(m);
		} else if (op == "update" || op == "delete") {
			auto id = obj["id"].toInteger();
			auto it = std::find_if(markers.begin(), markers.end(),
					       [id](const auto &m) { return m.id == id; });
			if (it == markers.end())
				return false;
			if (op == "delete") {
				ids.erase(it->id);
				markers.erase(it);
			} else {
				it->label = obj["label"].toString().toStdString();
				it->color = obj["color"].toString().toStdString();
				it->comment = obj["comment"].toString().toStdString();
				if (obj.contains("type_index"))
					it->type_index = qBound(0, obj["type_index"].toInt(), 3);
			}
		} else if (op == "clear") {
			markers.clear();
			ids.clear();
		} else if (op == "video_path")
			root["video_file_path"] = obj["video_path"];
		else
			return false;
	}
	if (file.error() != QFileDevice::NoError || !header)
		return false;
	QJsonArray array;
	for (const auto &m : markers)
		array.append(SessionCodec::encode_marker(m));
	root["markers"] = array;
	if (!SessionCodec::decode(root, session))
		return false;
	cache_path_ = path;
	source_path_.clear();
	return true;
}
