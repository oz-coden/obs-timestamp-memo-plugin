#include "srt-exporter.hpp"
#include "timecode-helper.hpp"

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

bool SrtExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
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
		out << QString::fromStdString(TimecodeHelper::ms_to_srt_time(start_ms)) << " --> "
		    << QString::fromStdString(TimecodeHelper::ms_to_srt_time(end_ms)) << "\n";

		QString text = "[" + QString::fromStdString(m.label) + "]";
		if (!m.comment.empty()) {
			text += " " + QString::fromStdString(m.comment);
		}
		out << text << "\n\n";
	}

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
