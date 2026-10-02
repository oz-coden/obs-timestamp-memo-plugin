#include "youtube-exporter.hpp"
#include "text-formats.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

bool YoutubeExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	QSaveFile file(QString::fromStdString(output_path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	QTextStream out(&file);

	std::string content = generate_chapters(session);
	out << QString::fromStdString(content);

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}

std::string YoutubeExporter::generate_chapters(const RecordingSession &session)
{
	return TextFormats::generate_chapters(session);
}
