#pragma once
#include "recording-session.hpp"
namespace TextFormats {
std::string generate_chapters(const RecordingSession &session);
std::string generate_markdown(const RecordingSession &session);
} // namespace TextFormats
