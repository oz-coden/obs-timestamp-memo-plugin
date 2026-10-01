#include "session-codec.hpp"
#include <QJsonArray>
#include <QFileInfo>
#include <utility>

QJsonObject SessionCodec::encode_marker(const MemoMarker &m)
{
	QJsonObject obj;
	obj["id"] = static_cast<qint64>(m.id);
	obj["timestamp_ms"] = static_cast<qint64>(m.timestamp_ms);
	obj["frame_index"] = static_cast<qint64>(m.frame_index);
	obj["timecode_ndf"] = QString::fromStdString(m.timecode_ndf);
	obj["timecode_df"] = QString::fromStdString(m.timecode_df);
	obj["type_index"] = m.type_index;
	obj["label"] = QString::fromStdString(m.label);
	obj["color"] = QString::fromStdString(m.color);
	obj["comment"] = QString::fromStdString(m.comment);
	obj["created_at_utc"] = QString::fromStdString(m.created_at_utc);
	obj["is_paused"] = m.is_paused;
	return obj;
}

MemoMarker SessionCodec::decode_marker(const QJsonObject &obj)
{
	MemoMarker m;
	qint64 raw_id = obj["id"].toInteger(0);
	qint64 raw_ms = obj["timestamp_ms"].toInteger(0);
	qint64 raw_frame = obj["frame_index"].toInteger(0);

	m.id = (raw_id > 0 && raw_id <= 100000000) ? static_cast<uint32_t>(raw_id) : 0;
	m.timestamp_ms = (raw_ms >= 0 && raw_ms <= 864000000) ? static_cast<uint64_t>(raw_ms) : 0;
	m.frame_index = (raw_frame >= 0 && raw_frame <= 1000000000) ? static_cast<uint64_t>(raw_frame) : 0;
	m.timecode_ndf = obj["timecode_ndf"].toString().toStdString();
	m.timecode_df = obj["timecode_df"].toString().toStdString();
	m.type_index = qBound(0, obj["type_index"].toInt(0), 3);
	m.label = obj["label"].toString().toStdString();
	m.color = obj["color"].toString("#3498db").toStdString();
	m.comment = obj["comment"].toString().toStdString();
	m.created_at_utc = obj["created_at_utc"].toString().toStdString();
	m.is_paused = obj["is_paused"].toBool(false);
	return m;
}

QJsonObject SessionCodec::encode(const RecordingSession &session)
{

	QJsonObject root;
	root["schema_version"] = "1.0.0";
	root["session_id"] = QString::fromStdString(session.session_id());
	root["video_file_path"] = QString::fromStdString(session.video_path());

	QFileInfo fi(QString::fromStdString(session.video_path()));
	root["video_file_name"] = fi.fileName();
	root["started_at_utc"] = QString::fromStdString(session.started_at());

	QJsonObject video_info;
	video_info["fps_num"] = static_cast<qint64>(session.frame_rate().num);
	video_info["fps_den"] = static_cast<qint64>(session.frame_rate().den);
	video_info["fps"] = session.frame_rate().fps();
	video_info["is_drop_frame"] = session.frame_rate().is_drop_frame();
	video_info["width"] = static_cast<qint64>(session.width());
	video_info["height"] = static_cast<qint64>(session.height());
	root["video_info"] = video_info;

	QJsonArray markers_array;
	for (const auto &m : session.get_markers()) {
		markers_array.append(encode_marker(m));
	}
	root["markers"] = markers_array;

	return root;
}

bool SessionCodec::decode(const QJsonObject &root, RecordingSession &session)
{
	if (!root["session_id"].isString() || root["session_id"].toString().isEmpty() ||
	    !root["video_info"].isObject() || !root["markers"].isArray())
		return false;
	SessionMetadata meta;
	meta.session_id = root["session_id"].toString().toStdString();
	meta.video_path = root["video_file_path"].toString().toStdString();
	meta.started_at = root["started_at_utc"].toString().toStdString();
	auto video = root["video_info"].toObject();
	qint64 num = video["fps_num"].toInteger(60), den = video["fps_den"].toInteger(1);
	if (num > 0 && num <= 1000000 && den > 0 && den <= 1000000) {
		VideoFrameRate fps{static_cast<uint32_t>(num), static_cast<uint32_t>(den)};
		if (fps.valid())
			meta.fps = fps;
	}
	qint64 w = video["width"].toInteger(1920), h = video["height"].toInteger(1080);
	meta.width = (w > 0 && w <= 32768) ? static_cast<uint32_t>(w) : 1920;
	meta.height = (h > 0 && h <= 32768) ? static_cast<uint32_t>(h) : 1080;
	std::vector<MemoMarker> markers;
	for (const auto value : root["markers"].toArray()) {
		if (!value.isObject())
			return false;
		markers.push_back(decode_marker(value.toObject()));
	}
	session.replace(std::move(meta), std::move(markers));
	return true;
}
