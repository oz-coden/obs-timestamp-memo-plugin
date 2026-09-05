#include "exporter-registry.hpp"
#include "csv-exporter.hpp"
#include "edl-exporter.hpp"
#include "json-exporter.hpp"
#include "markdown-exporter.hpp"
#include "srt-exporter.hpp"
#include "vtt-exporter.hpp"
#include "xml-exporter.hpp"
#include "youtube-exporter.hpp"

ExporterRegistry &ExporterRegistry::instance()
{
	static ExporterRegistry reg;
	return reg;
}

ExporterRegistry::ExporterRegistry()
{
	register_defaults();
}

void ExporterRegistry::register_defaults()
{
	register_exporter(std::make_shared<JsonExporter>());
	register_exporter(std::make_shared<CsvExporter>());
	register_exporter(std::make_shared<EdlExporter>());
	register_exporter(std::make_shared<SrtExporter>());
	register_exporter(std::make_shared<VttExporter>());
	register_exporter(std::make_shared<YoutubeExporter>());
	register_exporter(std::make_shared<MarkdownExporter>());
	register_exporter(std::make_shared<XmlExporter>());
}

void ExporterRegistry::register_exporter(std::shared_ptr<IExporter> exporter)
{
	if (!exporter)
		return;

	for (auto &existing : exporters_) {
		if (existing->get_id() == exporter->get_id()) {
			existing = exporter;
			return;
		}
	}
	exporters_.push_back(std::move(exporter));
}

std::vector<std::shared_ptr<IExporter>> ExporterRegistry::get_all() const
{
	return exporters_;
}

std::shared_ptr<IExporter> ExporterRegistry::find_by_id(const std::string &id) const
{
	for (const auto &exp : exporters_) {
		if (exp->get_id() == id) {
			return exp;
		}
	}
	return nullptr;
}

std::shared_ptr<IExporter> ExporterRegistry::find_by_extension(const std::string &ext) const
{
	std::string clean_ext = ext;
	if (!clean_ext.empty() && clean_ext[0] == '.') {
		clean_ext = clean_ext.substr(1);
	}

	for (const auto &exp : exporters_) {
		if (exp->get_file_extension() == clean_ext) {
			return exp;
		}
	}
	return nullptr;
}

bool ExporterRegistry::export_by_id(const std::string &id, const RecordingSession &session,
				    const std::string &output_path) const
{
	auto exp = find_by_id(id);
	if (!exp)
		return false;
	return exp->export_to_file(session, output_path);
}
