#pragma once
#include "recording-session.hpp"
#include <QJsonObject>
namespace SessionCodec {
QJsonObject encode_marker(const MemoMarker &marker);
bool decode_marker(const QJsonObject &object, const VideoFrameRate &fps, MemoMarker &marker, QString *error = nullptr);
bool update_marker(const QJsonObject &object, const VideoFrameRate &fps, MemoMarker &marker, QString *error = nullptr);
QJsonObject encode(const RecordingSession &session);
bool decode(const QJsonObject &object, RecordingSession &session, QString *error = nullptr);
} // namespace SessionCodec
