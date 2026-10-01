#include "youtube-exporter.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

static std::string format_youtube_timestamp(uint64_t ms, bool use_hours)
{
	uint64_t total_sec = ms / 1000;
	uint64_t sec = total_sec % 60;
	uint64_t min = (total_sec / 60) % 60;
	uint64_t hour = total_sec / 3600;

	char buf[32];
	if (use_hours) {
		std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu", static_cast<unsigned long long>(hour),
			      static_cast<unsigned long long>(min), static_cast<unsigned long long>(sec));
	} else {
		uint64_t display_min = total_sec / 60;
		std::snprintf(buf, sizeof(buf), "%02llu:%02llu", static_cast<unsigned long long>(display_min),
			      static_cast<unsigned long long>(sec));
	}
	return std::string(buf);
}

std::string YoutubeExporter::generate_chapters(const RecordingSession &session)
{
	auto markers = session.get_markers();
	std::stable_sort(markers.begin(), markers.end(),
			 [](const auto &a, const auto &b) { return a.timestamp_ms < b.timestamp_ms; });

	bool use_hours = false;
	for (const auto &m : markers) {
		if (m.timestamp_ms >= 3600000) {
			use_hours = true;
			break;
		}
	}

	std::string result;

	// YouTube chapters MUST start with 00:00 (or 00:00:00)
	bool has_start = false;
	if (!markers.empty() && markers[0].timestamp_ms < 1000) {
		has_start = true;
	}

	if (!has_start) {
		result += format_youtube_timestamp(0, use_hours) + " Start\n";
	}

	uint64_t previous_second = std::numeric_limits<uint64_t>::max();
	for (const auto &m : markers) {
		std::string title;
		if (!m.label.empty() && !m.comment.empty()) {
			title = m.label + " - " + m.comment;
		} else if (!m.label.empty()) {
			title = m.label;
		} else if (!m.comment.empty()) {
			title = m.comment;
		} else {
			title = "Chapter";
		}

		// Replace newlines in title with spaces to keep single-line per chapter
		std::replace(title.begin(), title.end(), '\n', ' ');
		std::replace(title.begin(), title.end(), '\r', ' ');

		if (m.timestamp_ms / 1000 == previous_second) {
			result.pop_back();
			result += " / " + title + "\n";
		} else {
			result += format_youtube_timestamp(m.timestamp_ms, use_hours) + " " + title + "\n";
		}
		previous_second = m.timestamp_ms / 1000;
	}

	return result;
}

bool YoutubeExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	QSaveFile file(QString::fromStdString(output_path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	out.setCodec("UTF-8");
#endif

	std::string content = generate_chapters(session);
	out << QString::fromStdString(content);

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
