#include "edl-exporter.hpp"
#include "timecode-helper.hpp"

#include <fstream>
#include <iomanip>
#include <QFileInfo>

bool EdlExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	std::ofstream out(output_path, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}

	QFileInfo fi(QString::fromStdString(session.video_path()));
	std::string clip_name = fi.fileName().toStdString();
	if (clip_name.empty()) {
		clip_name = "OBS_RECORDING";
	}

	auto fps = session.frame_rate();
	bool is_df = fps.is_drop_frame();

	// EDL Header
	out << "TITLE: " << fi.completeBaseName().toStdString() << "\n";
	out << "FCM: " << (is_df ? "DROP FRAME" : "NON-DROP FRAME") << "\n\n";

	auto markers = session.get_markers();
	int event_num = 1;

	for (const auto &m : markers) {
		std::string tc_in = m.active_timecode(fps);
		uint64_t next_frame = m.frame_index + 1;
		std::string tc_out = TimecodeHelper::frame_index_to_smpte(next_frame, fps, !is_df);

		// Event line: 001  AX  V  C  00:00:00:00 00:00:00:01 00:00:00:00 00:00:00:01
		out << std::setfill('0') << std::setw(3) << event_num++ << "  AX       V     C        "
		    << tc_in << " " << tc_out << " "
		    << tc_in << " " << tc_out << "\n";

		if (!clip_name.empty()) {
			out << "* FROM CLIP NAME: " << clip_name << "\n";
		}

		std::string label = m.label;
		if (label.empty())
			label = "Marker";
		out << "* MARKER NAME: " << label << "\n";

		if (!m.comment.empty()) {
			out << "* COMMENT: " << m.comment << "\n";
		}

		if (!m.color.empty()) {
			out << "* COLOR: " << m.color << "\n";
		}

		out << "\n";
	}

	out.close();
	return true;
}
