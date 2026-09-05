#pragma once

#include "exporter-base.hpp"

class VttExporter : public IExporter {
public:
	std::string get_id() const override { return "vtt"; }
	std::string get_format_name() const override { return "WebVTT Subtitle"; }
	std::string get_file_extension() const override { return "vtt"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
	bool can_export_to_string() const override { return true; }
	std::string export_to_string(const RecordingSession &session) override;
};
