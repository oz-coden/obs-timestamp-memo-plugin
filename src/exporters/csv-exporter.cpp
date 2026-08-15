#include "csv-exporter.hpp"

#include <fstream>
#include <sstream>

namespace {
std::string escape_csv(const std::string &str)
{
	std::string res = "\"";
	for (char c : str) {
		if (c == '"') {
			res += "\"\"";
		} else {
			res += c;
		}
	}
	res += "\"";
	return res;
}
} // namespace

bool CsvExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	std::ofstream out(output_path, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}

	// UTF-8 BOM
	out << "\xEF\xBB\xBF";

	// Header line
	out << "Index,Timecode In,Timecode Out,Frame In,Elapsed Ms,Marker Name,Comment,Color,Status\n";

	auto markers = session.get_markers();
	auto fps = session.frame_rate();

	for (const auto &m : markers) {
		std::string tc = m.active_timecode(fps);
		out << m.id << ","
		    << escape_csv(tc) << ","
		    << escape_csv(tc) << ","
		    << m.frame_index << ","
		    << m.timestamp_ms << ","
		    << escape_csv(m.label) << ","
		    << escape_csv(m.comment) << ","
		    << escape_csv(m.color) << ","
		    << (m.is_paused ? "\"PAUSED\"" : "\"NORMAL\"")
		    << "\n";
	}

	out.close();
	return true;
}
