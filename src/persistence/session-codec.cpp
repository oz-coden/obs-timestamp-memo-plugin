#include "session-codec.hpp"
#include <QJsonArray>
#include <QFileInfo>
#include <utility>
#include <QUuid>
#include <QDateTime>
#include <limits>
#include <unordered_set>
#include <cmath>
#include <QCoreApplication>
#include <QJsonDocument>

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
QString actual_value(const QJsonValue &value)
{
	if (value.isUndefined())
		return QCoreApplication::translate("TimestampMemo", "missing");
	const auto encoded = QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
	return encoded.mid(1, encoded.size() - 2).left(80);
}
bool invalid(QString *error, const QString &field, const QString &expected, const QJsonValue &value)
{
	if (error)
		*error = QCoreApplication::translate("TimestampMemo", "Field '%1': expected %2; actual %3.")
				 .arg(field, expected, actual_value(value));
	return false;
}
bool number(const QJsonObject &obj, const char *key, qint64 minimum, qint64 maximum, qint64 &value, QString *error)
{
	return integer(obj[key], minimum, maximum, value) ||
	       invalid(error, key,
		       QCoreApplication::translate("TimestampMemo", "an integer in [%1, %2]").arg(minimum).arg(maximum),
		       obj[key]);
}
bool typed(const QJsonObject &obj, const char *key, QJsonValue::Type type, const char *name, QString *error,
	   bool optional = false)
{
	return (optional && !obj.contains(key)) || obj[key].type() == type ||
	       invalid(error, key, QCoreApplication::translate("TimestampMemo", name), obj[key]);
}
} // namespace

bool SessionCodec::decode_marker(const QJsonObject &obj, const VideoFrameRate &fps, MemoMarker &marker, QString *error)
{
	if (error)
		error->clear();
	qint64 id = 0, ms = 0, frame = 0, type = 0;
	if (!number(obj, "id", 1, std::numeric_limits<uint32_t>::max(), id, error) ||
	    !number(obj, "timestamp_ms", 0, json_integer_max, ms, error) ||
	    !number(obj, "frame_index", 0, json_integer_max, frame, error) ||
	    !number(obj, "type_index", 0, 3, type, error))
		return false;
	for (const auto *key : {"label", "color", "comment"})
		if (!typed(obj, key, QJsonValue::String, "a string", error))
			return false;
	if (!typed(obj, "is_paused", QJsonValue::Bool, "a boolean", error))
		return false;
	for (const auto *key : {"created_at_utc", "timecode_ndf", "timecode_df"})
		if (!typed(obj, key, QJsonValue::String, "a string", error, true))
			return false;
	const auto expected_frame = TimecodeHelper::ms_to_frame_index(static_cast<uint64_t>(ms), fps);
	if (static_cast<uint64_t>(frame) != expected_frame)
		return invalid(error, "frame_index",
			       QCoreApplication::translate("TimestampMemo", "%1 (matching timestamp_ms and FPS)")
				       .arg(expected_frame),
			       obj["frame_index"]);
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
bool SessionCodec::update_marker(const QJsonObject &obj, const VideoFrameRate &fps, MemoMarker &marker, QString *error)
{
	qint64 id = 0;
	if (error)
		error->clear();
	if (!number(obj, "id", 1, std::numeric_limits<uint32_t>::max(), id, error))
		return false;
	if (id != marker.id)
		return invalid(error, "id", QString::number(marker.id), obj["id"]);
	auto merged = encode_marker(marker);
	bool changed = false;
	for (auto it = merged.begin(); it != merged.end(); ++it) {
		if (!obj.contains(it.key()))
			continue;
		if (it.key() == "label" || it.key() == "color" || it.key() == "comment" || it.key() == "type_index") {
			it.value() = obj[it.key()];
			changed = true;
		} else if (it.value() != obj[it.key()])
			return invalid(error, it.key(), actual_value(it.value()),
				       obj[it.key()]); // Identity/timing are immutable.
	}
	if (!changed)
		return invalid(error, "update",
			       QCoreApplication::translate("TimestampMemo", "at least one editable field"), obj);
	return decode_marker(merged, fps, marker, error);
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

bool SessionCodec::decode(const QJsonObject &root, RecordingSession &session, QString *error)
{
	if (error)
		error->clear();
	// Absent schema is the legacy format (including pre-schema journals).
	if (root.contains("schema_version") && root["schema_version"] != QJsonValue("1.0.0"))
		return invalid(error, "schema_version", "1.0.0", root["schema_version"]);
	for (const auto *key : {"session_id", "video_file_path", "started_at_utc"})
		if (!typed(root, key, QJsonValue::String, "a string", error))
			return false;
	if (QUuid(root["session_id"].toString()).isNull())
		return invalid(error, "session_id", QCoreApplication::translate("TimestampMemo", "a non-null UUID"),
			       root["session_id"]);
	if (!QDateTime::fromString(root["started_at_utc"].toString(), Qt::ISODateWithMs).isValid())
		return invalid(error, "started_at_utc",
			       QCoreApplication::translate("TimestampMemo", "an ISO 8601 date/time"),
			       root["started_at_utc"]);
	if (!typed(root, "video_info", QJsonValue::Object, "an object", error) ||
	    !typed(root, "markers", QJsonValue::Array, "an array", error))
		return false;
	SessionMetadata meta;
	meta.session_id = root["session_id"].toString().toStdString();
	meta.video_path = root["video_file_path"].toString().toStdString();
	meta.started_at = root["started_at_utc"].toString().toStdString();
	const auto video = root["video_info"].toObject();
	qint64 num = 0, den = 0, width = 0, height = 0;
	if (!number(video, "fps_num", 1, 1000000, num, error) || !number(video, "fps_den", 1, 1000000, den, error) ||
	    !number(video, "width", 1, 32768, width, error) || !number(video, "height", 1, 32768, height, error))
		return false;
	meta.fps = {static_cast<uint32_t>(num), static_cast<uint32_t>(den)};
	if (!meta.fps.valid())
		return invalid(error, "video_info",
			       QCoreApplication::translate("TimestampMemo", "FPS between 1 and 1000"), video);
	if (video.contains("fps") &&
	    (!video["fps"].isDouble() || std::abs(video["fps"].toDouble() - meta.fps.fps()) > 0.000001))
		return invalid(error, "fps", QString::number(meta.fps.fps(), 'g', 15), video["fps"]);
	if (video.contains("is_drop_frame") &&
	    (!video["is_drop_frame"].isBool() || video["is_drop_frame"].toBool() != meta.fps.is_drop_frame()))
		return invalid(error, "is_drop_frame", actual_value(meta.fps.is_drop_frame()), video["is_drop_frame"]);
	meta.width = static_cast<uint32_t>(width);
	meta.height = static_cast<uint32_t>(height);
	std::vector<MemoMarker> markers;
	std::unordered_set<uint32_t> ids;
	const auto array = root["markers"].toArray();
	markers.reserve(static_cast<size_t>(array.size()));
	int row = 0;
	for (const auto value : array) {
		++row;
		MemoMarker marker;
		QString detail;
		if (!value.isObject())
			invalid(&detail, "marker", QCoreApplication::translate("TimestampMemo", "an object"), value);
		else if (decode_marker(value.toObject(), meta.fps, marker, &detail) && !ids.insert(marker.id).second)
			invalid(&detail, "id", QCoreApplication::translate("TimestampMemo", "a unique marker ID"),
				value.toObject()["id"]);
		if (!detail.isEmpty()) {
			if (error)
				*error = QCoreApplication::translate("TimestampMemo", "Marker #%1 (ID %2): %3")
						 .arg(row)
						 .arg(actual_value(value.toObject()["id"]), detail);
			return false;
		}
		markers.push_back(std::move(marker));
	}
	session.replace(std::move(meta), std::move(markers));
	return true;
}
