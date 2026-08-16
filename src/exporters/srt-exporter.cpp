#include "srt-exporter.hpp"
#include "timecode-helper.hpp"

#include <algorithm>
#include <fstream>
#include <QDir>
#include <QFileInfo>

bool SrtExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	std::ofstream out(output_path, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}

	// UTF-8 BOM
	out << "\xEF\xBB\xBF";

	auto markers = session.get_markers();
	int cue_index = 1;

	for (size_t i = 0; i < markers.size(); ++i) {
		const auto &m = markers[i];
		uint64_t start_ms = m.timestamp_ms;
		uint64_t end_ms = start_ms + 2500;

		if (i + 1 < markers.size()) {
			end_ms = std::min(end_ms, markers[i + 1].timestamp_ms);
		}
		if (end_ms <= start_ms) {
			end_ms = start_ms + 1000;
		}

		out << cue_index++ << "\n";
		out << TimecodeHelper::ms_to_srt_time(start_ms) << " --> " << TimecodeHelper::ms_to_srt_time(end_ms)
		    << "\n";

		std::string text = "[" + m.label + "]";
		if (!m.comment.empty()) {
			text += " " + m.comment;
		}
		out << text << "\n\n";
	}

	out.flush();
	bool ok = !out.fail() && !out.bad();
	out.close();
	return ok;
}
