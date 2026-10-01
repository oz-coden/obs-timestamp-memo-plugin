#pragma once
#include "recording-session.hpp"
#include <QJsonObject>
namespace SessionCodec {
QJsonObject encode_marker(const MemoMarker &marker);
MemoMarker decode_marker(const QJsonObject &object);
QJsonObject encode(const RecordingSession &session);
bool decode(const QJsonObject &object, RecordingSession &session);
} // namespace SessionCodec
