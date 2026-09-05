#include "vtt-exporter.hpp"
#include "timecode-helper.hpp"

#include <algorithm>
#include <sstream>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

std::string VttExporter::export_to_string(const RecordingSession &session)
{
	std::ostringstream ss;
	ss << "WEBVTT\n\n";

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

		ss << cue_index++ << "\n";
		ss << TimecodeHelper::ms_to_timestamp_str(start_ms, true) << " --> "
		   << TimecodeHelper::ms_to_timestamp_str(end_ms, true) << "\n";

		ss << "[" << m.label << "]";
		if (!m.comment.empty()) {
			ss << " " << m.comment;
		}
		ss << "\n\n";
	}

	return ss.str();
}

bool VttExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	QSaveFile file(QString::fromStdString(output_path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	out.setCodec("UTF-8");
#endif

	std::string content = export_to_string(session);
	out << QString::fromStdString(content);

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
