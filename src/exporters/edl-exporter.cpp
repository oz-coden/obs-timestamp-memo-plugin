#include "edl-exporter.hpp"
#include "timecode-helper.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace {
QString clean_edl_qstring(const std::string &str)
{
	QString qstr = QString::fromStdString(str);
	qstr.replace('\r', ' ');
	qstr.replace('\n', ' ');
	qstr.replace('\t', ' ');
	return qstr;
}
} // namespace

bool EdlExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	QSaveFile file(QString::fromStdString(output_path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	QTextStream out(&file);

	QFileInfo src_fi(QString::fromStdString(session.video_path()));
	QString clip_name = src_fi.fileName();
	if (clip_name.isEmpty()) {
		clip_name = "OBS_RECORDING";
	}

	auto fps = session.frame_rate();
	bool is_df = fps.is_drop_frame();

	out << "TITLE: " << clean_edl_qstring(src_fi.completeBaseName().toStdString()) << "\n";
	out << "FCM: " << (is_df ? "DROP FRAME" : "NON-DROP FRAME") << "\n\n";

	const auto &markers = session.get_markers();
	int event_num = 1;

	for (const auto &m : markers) {
		QString tc_in = QString::fromStdString(m.active_timecode(fps));
		uint64_t next_frame = m.frame_index + 1;
		QString tc_out = QString::fromStdString(TimecodeHelper::frame_index_to_smpte(next_frame, fps, !is_df));

		out << QString("%1").arg(event_num++, 3, 10, QChar('0')) << "  AX       V     C        " << tc_in << " "
		    << tc_out << " " << tc_in << " " << tc_out << "\n";

		if (!clip_name.isEmpty()) {
			out << "* FROM CLIP NAME: " << clean_edl_qstring(clip_name.toStdString()) << "\n";
		}

		std::string label = m.label.empty() ? "Marker" : m.label;
		out << "* MARKER NAME: " << clean_edl_qstring(label) << "\n";

		if (!m.comment.empty()) {
			out << "* COMMENT: " << clean_edl_qstring(m.comment) << "\n";
		}

		if (!m.color.empty()) {
			out << "* COLOR: " << clean_edl_qstring(m.color) << "\n";
		}

		out << "\n";
	}

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
