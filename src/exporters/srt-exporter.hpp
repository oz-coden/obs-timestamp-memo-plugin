#pragma once

#include "exporter-base.hpp"

class SrtExporter : public IExporter {
public:
	std::string get_format_name() const override
	{
		return "SubRip Subtitle (*.srt)";
	}
	std::string get_file_extension() const override
	{
		return "srt";
	}
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
};
