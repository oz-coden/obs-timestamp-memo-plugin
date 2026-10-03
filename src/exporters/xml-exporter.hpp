#pragma once

#include "exporter-base.hpp"

class XmlExporter : public IExporter {
public:
	std::string get_id() const override { return "xml"; }
	std::string get_format_name() const override { return "FCP-style XML (NLE compatibility unverified)"; }
	std::string get_file_extension() const override { return "xml"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
};
