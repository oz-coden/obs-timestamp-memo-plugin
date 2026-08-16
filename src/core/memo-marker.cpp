#include "memo-marker.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <QDateTime>

namespace {
std::string get_current_iso_time()
{
	return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}
} // namespace

MemoMarker MemoMarker::create(uint32_t id, uint64_t ms, const VideoFrameRate &fps, int type_index,
			      const std::string &label, const std::string &color, const std::string &comment,
			      bool is_paused)
{
	MemoMarker m;
	m.id = id;
	m.timestamp_ms = ms;
	m.frame_index = TimecodeHelper::ms_to_frame_index(ms, fps);
	m.timecode_ndf = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps, true);
	m.timecode_df = TimecodeHelper::frame_index_to_smpte(m.frame_index, fps, false);
	m.type_index = (type_index < 0 || type_index >= 4) ? 0 : type_index;
	m.label = label;
	m.color = color.empty() ? "#3498db" : color;
	m.comment = comment;
	m.created_at_utc = get_current_iso_time();
	m.is_paused = is_paused;
	return m;
}

QJsonObject MemoMarker::to_json() const
{
	QJsonObject obj;
	obj["id"] = static_cast<qint64>(id);
	obj["timestamp_ms"] = static_cast<qint64>(timestamp_ms);
	obj["frame_index"] = static_cast<qint64>(frame_index);
	obj["timecode_ndf"] = QString::fromStdString(timecode_ndf);
	obj["timecode_df"] = QString::fromStdString(timecode_df);
	obj["type_index"] = type_index;
	obj["label"] = QString::fromStdString(label);
	obj["color"] = QString::fromStdString(color);
	obj["comment"] = QString::fromStdString(comment);
	obj["created_at_utc"] = QString::fromStdString(created_at_utc);
	obj["is_paused"] = is_paused;
	return obj;
}

MemoMarker MemoMarker::from_json(const QJsonObject &obj)
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

std::string MemoMarker::active_timecode(const VideoFrameRate &fps) const
{
	if (fps.is_drop_frame()) {
		return timecode_df;
	}
	return timecode_ndf;
}
