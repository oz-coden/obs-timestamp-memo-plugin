#pragma once

#include "exporter-base.hpp"

class XmlExporter : public IExporter {
public:
	std::string get_format_name() const override { return "Premiere Pro / FCP XML (*.xml)"; }
	std::string get_file_extension() const override { return "xml"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
};
