#include "vtt-exporter.hpp"
#include "timecode-helper.hpp"
#include "text-formats.hpp"

#include <sstream>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

std::string VttExporter::export_to_string(const RecordingSession &session)
{
	std::ostringstream ss;
	ss << "WEBVTT\n\n";

	int cue_index = 1;
	for (const auto &cue : TextFormats::subtitle_cues(session)) {
		ss << cue_index++ << "\n";
		ss << TimecodeHelper::ms_to_timestamp_str(cue.start_ms, true) << " --> "
		   << TimecodeHelper::ms_to_timestamp_str(cue.end_ms, true) << "\n";

		QString payload = QString::fromStdString(cue.text);
		payload.replace('&', "&amp;");
		payload.replace('<', "&lt;");
		payload.replace('>', "&gt;");
		payload.replace('\r', ' ');
		payload.replace('\n', ' ');
		ss << payload.toStdString();
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

	std::string content = export_to_string(session);
	out << QString::fromStdString(content);

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
