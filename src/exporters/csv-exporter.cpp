#include "csv-exporter.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace {
QString escape_csv_field(const std::string &str)
{
	QString qstr = QString::fromStdString(str);
	const auto trimmed = qstr.trimmed();
	if (qstr.startsWith('\'') || (!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front())))
		qstr.prepend('\''); // Export-only text prefix; the document remains unchanged.
	qstr.replace("\"", "\"\"");
	return "\"" + qstr + "\"";
}
} // namespace

bool CsvExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
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

	out << "Index,Marker ID,Timecode In,Timecode Out,Frame In,Elapsed Ms,Marker Name,Comment,Color,Status\n";

	const auto &markers = session.get_markers();
	auto fps = session.frame_rate();

	for (size_t index = 0; index < markers.size(); ++index) {
		const auto &m = markers[index];
		QString tc = QString::fromStdString(m.active_timecode(fps));
		QString esc_tc = "\"" + tc + "\"";
		out << (index + 1) << "," << m.id << "," << esc_tc << "," << esc_tc << "," << m.frame_index << ","
		    << m.timestamp_ms << "," << escape_csv_field(m.label) << "," << escape_csv_field(m.comment) << ","
		    << escape_csv_field(m.color) << "," << (m.is_paused ? "\"PAUSED\"" : "\"NORMAL\"") << "\n";
	}

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
