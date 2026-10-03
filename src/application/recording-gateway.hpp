#pragma once
#include "timecode-helper.hpp"
#include <QObject>
#include <string>
struct RecordingSnapshot {
	bool recording = false, paused = false;
	uint64_t total_frames = 0;
	std::string path;
	VideoFrameRate fps{60, 1};
	uint32_t width = 1920, height = 1080;
};
class RecordingGateway : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	virtual void initialize() = 0;
	virtual void shutdown() = 0;
	virtual RecordingSnapshot snapshot() const = 0;
	virtual void drain_pending_events() = 0;
signals:
	void recordingStarted();
	void recordingPaused();
	void recordingUnpaused();
	void recordingStopped();
	void recordingFileChanged(const QString &path, uint64_t total_frames);
	void quickMarkerRequested(int index);
	void focusMemoRequested();
	void integrationWarning(const QString &message);
};
