#include "edl-exporter.hpp"
#include "timecode-helper.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <QDir>
#include <QFileInfo>

namespace {
std::string clean_edl_text(const std::string &str)
{
	std::string res;
	res.reserve(str.size());
	for (char c : str) {
		if (c == '\r' || c == '\n' || c == '\t') {
			res += ' ';
		} else {
			res += c;
		}
	}
	return res;
}
} // namespace

bool EdlExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	std::ofstream out(output_path, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}

	QFileInfo src_fi(QString::fromStdString(session.video_path()));
	std::string clip_name = src_fi.fileName().toStdString();
	if (clip_name.empty()) {
		clip_name = "OBS_RECORDING";
	}

	auto fps = session.frame_rate();
	bool is_df = fps.is_drop_frame();

	out << "TITLE: " << clean_edl_text(src_fi.completeBaseName().toStdString()) << "\n";
	out << "FCM: " << (is_df ? "DROP FRAME" : "NON-DROP FRAME") << "\n\n";

	auto markers = session.get_markers();
	int event_num = 1;

	for (const auto &m : markers) {
		std::string tc_in = m.active_timecode(fps);
		uint64_t next_frame = m.frame_index + 1;
		std::string tc_out = TimecodeHelper::frame_index_to_smpte(next_frame, fps, !is_df);

		out << std::setfill('0') << std::setw(3) << event_num++ << "  AX       V     C        " << tc_in << " "
		    << tc_out << " " << tc_in << " " << tc_out << "\n";

		if (!clip_name.empty()) {
			out << "* FROM CLIP NAME: " << clean_edl_text(clip_name) << "\n";
		}

		std::string label = m.label.empty() ? "Marker" : m.label;
		out << "* MARKER NAME: " << clean_edl_text(label) << "\n";

		if (!m.comment.empty()) {
			out << "* COMMENT: " << clean_edl_text(m.comment) << "\n";
		}

		if (!m.color.empty()) {
			out << "* COLOR: " << clean_edl_text(m.color) << "\n";
		}

		out << "\n";
	}

	out.close();
	return true;
}
