#include "srt-exporter.hpp"
#include "timecode-helper.hpp"
#include "text-formats.hpp"

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

	// UTF-8 BOM
	out.setGenerateByteOrderMark(true);

	int cue_index = 1;
	for (const auto &cue : TextFormats::subtitle_cues(session)) {
		out << cue_index++ << "\n";
		out << QString::fromStdString(TimecodeHelper::ms_to_srt_time(cue.start_ms)) << " --> "
		    << QString::fromStdString(TimecodeHelper::ms_to_srt_time(cue.end_ms)) << "\n";

		QString text = QString::fromStdString(cue.text);
		text.replace('\r', ' ');
		text.replace('\n', ' ');
		out << text << "\n\n";
	}

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
