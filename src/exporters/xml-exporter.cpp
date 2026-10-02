#include "xml-exporter.hpp"

#include <cmath>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

namespace {
QString escape_xml_qstring(const std::string &str)
{
	QString qstr = QString::fromStdString(str);
	QString valid;
	for (QChar c : qstr) {
		const auto code = c.unicode();
		if ((code >= 0x20 || code == 0x09 || code == 0x0a || code == 0x0d) && code != 0xfffe && code != 0xffff)
			valid += c;
	}
	qstr = valid;
	qstr.replace('&', "&amp;");
	qstr.replace('<', "&lt;");
	qstr.replace('>', "&gt;");
	qstr.replace('\"', "&quot;");
	qstr.replace('\'', "&apos;");
	return qstr;
}
} // namespace

bool XmlExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
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

	QFileInfo src_fi(QString::fromStdString(session.video_path()));
	QString seq_name = src_fi.completeBaseName();
	if (seq_name.isEmpty()) {
		seq_name = "OBS Recording Markers";
	}

	auto fps = session.frame_rate();
	uint32_t timebase = static_cast<uint32_t>(std::round(fps.fps()));
	if (timebase == 0)
		timebase = 30;
	bool is_ntsc = fps.is_drop_frame() || (fps.den == 1001);

	out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	out << "<!DOCTYPE xmeml>\n";
	out << "<xmeml version=\"4\">\n";
	out << "  <sequence id=\"sequence-1\">\n";
	out << "    <name>" << escape_xml_qstring(seq_name.toStdString()) << "</name>\n";
	out << "    <rate>\n";
	out << "      <timebase>" << timebase << "</timebase>\n";
	out << "      <ntsc>" << (is_ntsc ? "TRUE" : "FALSE") << "</ntsc>\n";
	out << "    </rate>\n";
	out << "    <media>\n";
	out << "      <video>\n";
	out << "        <format>\n";
	out << "          <samplecharacteristics>\n";
	out << "            <width>" << session.width() << "</width>\n";
	out << "            <height>" << session.height() << "</height>\n";
	out << "            <rate>\n";
	out << "              <timebase>" << timebase << "</timebase>\n";
	out << "              <ntsc>" << (is_ntsc ? "TRUE" : "FALSE") << "</ntsc>\n";
	out << "            </rate>\n";
	out << "          </samplecharacteristics>\n";
	out << "        </format>\n";
	out << "      </video>\n";
	out << "    </media>\n";

	const auto &markers = session.get_markers();
	for (const auto &m : markers) {
		out << "    <marker>\n";
		out << "      <name>" << escape_xml_qstring(m.label) << "</name>\n";
		out << "      <comment>" << escape_xml_qstring(m.comment) << "</comment>\n";
		out << "      <in>" << m.frame_index << "</in>\n";
		out << "      <out>" << (m.frame_index + 1) << "</out>\n";
		out << "      <color>" << escape_xml_qstring(m.color) << "</color>\n";
		out << "    </marker>\n";
	}

	out << "  </sequence>\n";
	out << "</xmeml>\n";

	out.flush();
	if (out.status() != QTextStream::Ok) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
