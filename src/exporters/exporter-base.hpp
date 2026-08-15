#pragma once

#include "recording-session.hpp"
#include <string>

class IExporter {
public:
	virtual ~IExporter() = default;
	virtual std::string get_format_name() const = 0;
	virtual std::string get_file_extension() const = 0;
	virtual bool export_to_file(const RecordingSession &session, const std::string &output_path) = 0;
};
