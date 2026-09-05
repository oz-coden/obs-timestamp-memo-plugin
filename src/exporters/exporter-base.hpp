#pragma once

#include "recording-session.hpp"
#include <string>

class IExporter {
public:
	virtual ~IExporter() = default;
	virtual std::string get_id() const = 0;
	virtual std::string get_format_name() const = 0;
	virtual std::string get_file_extension() const = 0;
	virtual std::string get_filter_string() const
	{
		return get_format_name() + " (*." + get_file_extension() + ")";
	}
	virtual bool export_to_file(const RecordingSession &session, const std::string &output_path) = 0;

	virtual bool can_export_to_string() const { return false; }
	virtual std::string export_to_string(const RecordingSession &) { return ""; }
};
