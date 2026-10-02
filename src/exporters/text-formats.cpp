#include "text-formats.hpp"
#include "timecode-helper.hpp"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>

std::vector<TextFormats::SubtitleCue> TextFormats::subtitle_cues(const RecordingSession &session)
{
	auto markers = session.get_markers(); // Sorting must not modify the document.
	std::stable_sort(markers.begin(), markers.end(),
			 [](const auto &a, const auto &b) { return a.timestamp_ms < b.timestamp_ms; });
	std::vector<SubtitleCue> cues;
	cues.reserve(markers.size());
	auto end_after = [](uint64_t start, uint64_t duration) {
		return start + std::min(duration, std::numeric_limits<uint64_t>::max() - start);
	};
	for (size_t i = 0; i < markers.size(); ++i) {
		const auto &m = markers[i];
		auto end = end_after(m.timestamp_ms, 2500);
		if (i + 1 < markers.size())
			end = std::min(end, markers[i + 1].timestamp_ms);
		if (end <= m.timestamp_ms)
			end = end_after(m.timestamp_ms, 1000);
		std::string text = "[" + m.label + "]";
		if (!m.comment.empty())
			text += " " + m.comment;
		cues.push_back({m.timestamp_ms, end, std::move(text)});
	}
	return cues;
}

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

std::string TextFormats::generate_chapters(const RecordingSession &session)
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

static std::string escape_markdown_cell(std::string text)
{
	std::string result;
	result.reserve(text.size());
	for (char c : text) {
		if (c == '|') {
			result += "\\|";
		} else if (c == '\r' || c == '\n') {
			result += ' ';
		} else {
			result += c;
		}
	}
	return result;
}

std::string TextFormats::generate_markdown(const RecordingSession &session)
{
	std::ostringstream ss;

	std::string started = session.started_at();
	if (started.empty()) {
		started = "Unknown Session";
	}

	ss << "# Recording Memo - " << started << "\n\n";

	ss << "## Session Information\n\n";
	if (!session.video_path().empty()) {
		ss << "- **Video File**: `" << session.video_path() << "`\n";
	}
	ss << "- **Started At**: " << started << "\n";
	ss << "- **Resolution**: " << session.width() << "x" << session.height() << "\n";

	auto fps = session.frame_rate();
	ss << std::fixed << std::setprecision(2);
	ss << "- **Frame Rate**: " << fps.fps() << " fps (" << fps.num << "/" << fps.den << ")\n";

	const auto &markers = session.get_markers();
	ss << "- **Total Markers**: " << markers.size() << "\n\n";

	ss << "## Timeline / Chapters\n\n";
	if (markers.empty()) {
		ss << "_No markers recorded._\n\n";
	} else {
		for (const auto &m : markers) {
			std::string time_str = TimecodeHelper::ms_to_timestamp_str(m.timestamp_ms, false);
			ss << "- `" << time_str << "` ";
			if (!m.label.empty()) {
				ss << "**" << m.label << "**";
			}
			if (!m.comment.empty()) {
				if (!m.label.empty()) {
					ss << " - ";
				}
				ss << m.comment;
			}
			ss << "\n";
		}
		ss << "\n";
	}

	ss << "## Detailed Marker List\n\n";
	ss << "| # | Timestamp | SMPTE | Label | Comment | Color |\n";
	ss << "|---|-----------|-------|-------|---------|-------|\n";

	for (size_t i = 0; i < markers.size(); ++i) {
		const auto &m = markers[i];
		std::string ts = TimecodeHelper::ms_to_timestamp_str(m.timestamp_ms, true);
		std::string smpte = TimecodeHelper::ms_to_smpte(m.timestamp_ms, fps);

		ss << "| " << (i + 1) << " "
		   << "| `" << ts << "` "
		   << "| `" << smpte << "` "
		   << "| " << escape_markdown_cell(m.label) << " "
		   << "| " << escape_markdown_cell(m.comment) << " "
		   << "| `" << m.color << "` |\n";
	}

	ss << "\n";
	return ss.str();
}
