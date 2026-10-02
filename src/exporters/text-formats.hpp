#pragma once
#include "recording-session.hpp"
namespace TextFormats {
struct SubtitleCue {
	uint64_t start_ms, end_ms;
	std::string text;
};
std::vector<SubtitleCue> subtitle_cues(const RecordingSession &session);
std::string generate_chapters(const RecordingSession &session);
std::string generate_markdown(const RecordingSession &session);
} // namespace TextFormats
