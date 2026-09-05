#pragma once

#include "exporter-base.hpp"

class MarkdownExporter : public IExporter {
public:
	std::string get_id() const override { return "markdown"; }
	std::string get_format_name() const override { return "Markdown Document"; }
	std::string get_file_extension() const override { return "md"; }
	bool export_to_file(const RecordingSession &session, const std::string &output_path) override;
	bool can_export_to_string() const override { return true; }
	std::string export_to_string(const RecordingSession &session) override { return generate_markdown(session); }

	static std::string generate_markdown(const RecordingSession &session);
};
