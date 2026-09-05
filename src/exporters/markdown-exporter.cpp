#include "markdown-exporter.hpp"
#include "timecode-helper.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

static std::string escape_markdown_cell(std::string text)
{
	std::string result;
	result.reserve(text.size());
	for (char c : text) {
		if (c == '|') {
			result += "\\|";
		} else if (c == '\r' || c == '\n') {
			result += ' ';
		} else {
			result += c;
		}
	}
	return result;
}

std::string MarkdownExporter::generate_markdown(const RecordingSession &session)
{
	std::ostringstream ss;

	std::string started = session.started_at();
	if (started.empty()) {
		started = "Unknown Session";
	}

	ss << "# Recording Memo - " << started << "\n\n";

	ss << "## Session Information\n\n";
	if (!session.video_path().empty()) {
		ss << "- **Video File**: `" << session.video_path() << "`\n";
	}
	ss << "- **Started At**: " << started << "\n";
	ss << "- **Resolution**: " << session.width() << "x" << session.height() << "\n";

	auto fps = session.frame_rate();
	ss << std::fixed << std::setprecision(2);
	ss << "- **Frame Rate**: " << fps.fps() << " fps (" << fps.num << "/" << fps.den << ")\n";

	auto markers = session.get_markers();
	ss << "- **Total Markers**: " << markers.size() << "\n\n";

	ss << "## Timeline / Chapters\n\n";
	if (markers.empty()) {
		ss << "_No markers recorded._\n\n";
	} else {
		for (const auto &m : markers) {
			std::string time_str = TimecodeHelper::ms_to_timestamp_str(m.timestamp_ms, false);
			ss << "- `" << time_str << "` ";
			if (!m.label.empty()) {
				ss << "**" << m.label << "**";
			}
			if (!m.comment.empty()) {
				if (!m.label.empty()) {
					ss << " - ";
				}
				ss << m.comment;
			}
			ss << "\n";
		}
		ss << "\n";
	}

	ss << "## Detailed Marker List\n\n";
	ss << "| # | Timestamp | SMPTE | Label | Comment | Color |\n";
	ss << "|---|-----------|-------|-------|---------|-------|\n";

	for (size_t i = 0; i < markers.size(); ++i) {
		const auto &m = markers[i];
		std::string ts = TimecodeHelper::ms_to_timestamp_str(m.timestamp_ms, true);
		std::string smpte = TimecodeHelper::ms_to_smpte(m.timestamp_ms, fps);

		ss << "| " << (i + 1) << " "
		   << "| `" << ts << "` "
		   << "| `" << smpte << "` "
		   << "| " << escape_markdown_cell(m.label) << " "
		   << "| " << escape_markdown_cell(m.comment) << " "
		   << "| `" << m.color << "` |\n";
	}

	ss << "\n";
	return ss.str();
}

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
