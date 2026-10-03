#pragma once
#include "recording-session.hpp"
#include <memory>
#include <QFile>
#include <QJsonObject>
#include <QTextStream>
// UI-thread confined disk state; document and settings have no file ownership.
class SessionStore {
public:
	explicit SessionStore(std::string cache_directory = "") : cache_directory_(std::move(cache_directory)) {}
	bool begin(const RecordingSession &session);
	bool finish(const RecordingSession &session, bool save_json, std::string *path = nullptr);
	bool save(const RecordingSession &session, const std::string &path = "");
	bool load(const std::string &path, RecordingSession &session);
	bool recover(const std::string &path, RecordingSession &session);
	// Commit a validated read without repeating I/O or losing the cache directory.
	bool adopt_read(SessionStore &&reader);
	bool append(const QJsonObject &operation);
	bool journal_healthy() const { return healthy_; }
	bool journaling() const { return file_ != nullptr; }
	const std::string &source_path() const { return source_path_; }
	const std::string &cache_path() const { return cache_path_; }
	std::string create_recovery_copy(const RecordingSession &session) const;
	std::vector<std::string> recovery_copies() const;
	void acknowledge_saved(const std::string &path);
	bool discard_cache();
	bool save_recovery_snapshot(const RecordingSession &session);

private:
	void close();
	void remove_cache();
	std::string cache_directory_, cache_path_, source_path_;
	std::unique_ptr<QFile> file_;
	std::unique_ptr<QTextStream> stream_;
	bool healthy_ = false;
};
