#include "session-codec.hpp"
#include <QJsonArray>
#include <QFileInfo>
#include <utility>
#include <QUuid>
#include <QDateTime>
#include <limits>
#include <unordered_set>
#include <cmath>

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

namespace {
// Integers interoperable with JSON consumers that use IEEE-754 doubles.
// This is a numeric precision boundary, not a limit on recording duration.
constexpr qint64 json_integer_max = 9007199254740991LL;
bool integer(const QJsonValue &value, qint64 minimum, qint64 maximum, qint64 &result)
{
	if (!value.isDouble())
		return false;
	const auto n = value.toInteger(-1);
	if (n < minimum || n > maximum || value != QJsonValue(n))
		return false;
	result = n;
	return true;
}
bool optional_string(const QJsonObject &object, const char *key)
{
	return !object.contains(key) || object[key].isString();
}
} // namespace

bool SessionCodec::decode_marker(const QJsonObject &obj, const VideoFrameRate &fps, MemoMarker &marker)
{
	qint64 id = 0, ms = 0, frame = 0, type = 0;
	if (!integer(obj["id"], 1, std::numeric_limits<uint32_t>::max(), id) ||
	    !integer(obj["timestamp_ms"], 0, json_integer_max, ms) ||
	    !integer(obj["frame_index"], 0, json_integer_max, frame) || !integer(obj["type_index"], 0, 3, type) ||
	    !obj["label"].isString() || !obj["color"].isString() || !obj["comment"].isString() ||
	    !obj["is_paused"].isBool() || !optional_string(obj, "created_at_utc") ||
	    !optional_string(obj, "timecode_ndf") || !optional_string(obj, "timecode_df") ||
	    static_cast<uint64_t>(frame) != TimecodeHelper::ms_to_frame_index(static_cast<uint64_t>(ms), fps))
		return false;
	MemoMarker m;
	m.id = static_cast<uint32_t>(id);
	m.timestamp_ms = static_cast<uint64_t>(ms);
	m.frame_index = static_cast<uint64_t>(frame);
	m.type_index = static_cast<int>(type);
	m.label = obj["label"].toString().toStdString();
	m.color = obj["color"].toString().toStdString();
	m.comment = obj["comment"].toString().toStdString();
	m.created_at_utc = obj["created_at_utc"].toString().toStdString();
	m.is_paused = obj["is_paused"].toBool();
	m.timecode_ndf = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps, true);
	m.timecode_df = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps);
	marker = std::move(m);
	return true;
}
bool SessionCodec::update_marker(const QJsonObject &obj, const VideoFrameRate &fps, MemoMarker &marker)
{
	qint64 id = 0;
	if (!integer(obj["id"], 1, std::numeric_limits<uint32_t>::max(), id) || id != marker.id)
		return false;
	auto merged = encode_marker(marker);
	bool changed = false;
	for (auto it = merged.begin(); it != merged.end(); ++it) {
		if (!obj.contains(it.key()))
			continue;
		if (it.key() == "label" || it.key() == "color" || it.key() == "comment" || it.key() == "type_index") {
			it.value() = obj[it.key()];
			changed = true;
		} else if (it.value() != obj[it.key()])
			return false; // Updates cannot silently alter marker identity/timing.
	}
	return changed && decode_marker(merged, fps, marker);
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
	// Absent schema is the legacy format (including pre-schema journals).
	if ((root.contains("schema_version") && root["schema_version"] != QJsonValue("1.0.0")) ||
	    !root["session_id"].isString() || QUuid(root["session_id"].toString()).isNull() ||
	    !root["video_file_path"].isString() || !root["started_at_utc"].isString() ||
	    !QDateTime::fromString(root["started_at_utc"].toString(), Qt::ISODateWithMs).isValid() ||
	    !root["video_info"].isObject() || !root["markers"].isArray())
		return false;
	SessionMetadata meta;
	meta.session_id = root["session_id"].toString().toStdString();
	meta.video_path = root["video_file_path"].toString().toStdString();
	meta.started_at = root["started_at_utc"].toString().toStdString();
	const auto video = root["video_info"].toObject();
	qint64 num = 0, den = 0, width = 0, height = 0;
	if (!integer(video["fps_num"], 1, 1000000, num) || !integer(video["fps_den"], 1, 1000000, den) ||
	    !integer(video["width"], 1, 32768, width) || !integer(video["height"], 1, 32768, height))
		return false;
	meta.fps = {static_cast<uint32_t>(num), static_cast<uint32_t>(den)};
	if (!meta.fps.valid() ||
	    (video.contains("fps") &&
	     (!video["fps"].isDouble() || std::abs(video["fps"].toDouble() - meta.fps.fps()) > 0.000001)) ||
	    (video.contains("is_drop_frame") &&
	     (!video["is_drop_frame"].isBool() || video["is_drop_frame"].toBool() != meta.fps.is_drop_frame())))
		return false;
	meta.width = static_cast<uint32_t>(width);
	meta.height = static_cast<uint32_t>(height);
	std::vector<MemoMarker> markers;
	std::unordered_set<uint32_t> ids;
	const auto array = root["markers"].toArray();
	markers.reserve(static_cast<size_t>(array.size()));
	for (const auto value : array) {
		MemoMarker marker;
		if (!value.isObject() || !decode_marker(value.toObject(), meta.fps, marker) ||
		    !ids.insert(marker.id).second)
			return false;
		markers.push_back(std::move(marker));
	}
	session.replace(std::move(meta), std::move(markers));
	return true;
}
