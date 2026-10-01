#include "markdown-exporter.hpp"
#include "text-formats.hpp"
#include "timecode-helper.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

bool MarkdownExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
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

	std::string content = generate_markdown(session);
	out << QString::fromStdString(content);

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}

std::string MarkdownExporter::generate_markdown(const RecordingSession &session)
{
	return TextFormats::generate_markdown(session);
}
