#pragma once

#include "exporter-base.hpp"

class CsvExporter : public IExporter {
public:
	std::string get_format_name() const override { return "CSV (*.csv)"; }
	std::string get_file_extension() const override { return "csv"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
};
