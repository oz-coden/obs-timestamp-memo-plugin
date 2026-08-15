#pragma once

#include "exporter-base.hpp"

class EdlExporter : public IExporter {
public:
	std::string get_format_name() const override { return "CMX 3600 EDL (*.edl)"; }
	std::string get_file_extension() const override { return "edl"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
};
