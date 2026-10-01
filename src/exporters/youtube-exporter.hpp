#pragma once

#include "exporter-base.hpp"

class YoutubeExporter : public IExporter {
public:
	std::string get_id() const override { return "youtube"; }
	std::string get_format_name() const override { return "YouTube Chapters"; }
	std::string get_file_extension() const override { return "chapters.txt"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
	bool can_export_to_string() const override { return true; }
	std::string export_to_string(const RecordingSession &session) override { return generate_chapters(session); }

	static std::string generate_chapters(const RecordingSession &session);
};
