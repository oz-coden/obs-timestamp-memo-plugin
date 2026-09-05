#pragma once

#include "exporter-base.hpp"

class JsonExporter : public IExporter {
public:
	std::string get_id() const override { return "json"; }
	std::string get_format_name() const override { return "JSON File"; }
	std::string get_file_extension() const override { return "json"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
};
