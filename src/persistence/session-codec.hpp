#pragma once
#include "recording-session.hpp"
#include <QJsonObject>
namespace SessionCodec {
QJsonObject encode_marker(const MemoMarker &marker);
bool decode_marker(const QJsonObject &object, const VideoFrameRate &fps, MemoMarker &marker);
bool update_marker(const QJsonObject &object, const VideoFrameRate &fps, MemoMarker &marker);
QJsonObject encode(const RecordingSession &session);
bool decode(const QJsonObject &object, RecordingSession &session);
} // namespace SessionCodec
