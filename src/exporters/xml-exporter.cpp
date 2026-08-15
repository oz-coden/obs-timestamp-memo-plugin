#include "xml-exporter.hpp"

#include <cmath>
#include <fstream>
#include <QFileInfo>
#include <QString>

namespace {
std::string escape_xml(const std::string &str)
{
	std::string res;
	for (char c : str) {
		switch (c) {
		case '&':
			res += "&amp;";
			break;
		case '<':
			res += "&lt;";
			break;
		case '>':
			res += "&gt;";
			break;
		case '\"':
			res += "&quot;";
			break;
		case '\'':
			res += "&apos;";
			break;
		default:
			res += c;
			break;
		}
	}
	return res;
}
} // namespace

bool XmlExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	std::ofstream out(output_path, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}

	QFileInfo fi(QString::fromStdString(session.video_path()));
	std::string seq_name = fi.completeBaseName().toStdString();
	if (seq_name.empty()) {
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
	out << "    <name>" << escape_xml(seq_name) << "</name>\n";
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

	auto markers = session.get_markers();
	for (const auto &m : markers) {
		out << "    <marker>\n";
		out << "      <name>" << escape_xml(m.label) << "</name>\n";
		out << "      <comment>" << escape_xml(m.comment) << "</comment>\n";
		out << "      <in>" << m.frame_index << "</in>\n";
		out << "      <out>" << (m.frame_index + 1) << "</out>\n";
		out << "      <color>" << escape_xml(m.color) << "</color>\n";
		out << "    </marker>\n";
	}

	out << "  </sequence>\n";
	out << "</xmeml>\n";

	out.close();
	return true;
}
