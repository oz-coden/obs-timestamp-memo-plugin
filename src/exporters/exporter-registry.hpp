#pragma once

#include "exporter-base.hpp"
#include <memory>
#include <string>
#include <vector>

class ExporterRegistry {
public:
	static ExporterRegistry &instance();

	void register_exporter(std::shared_ptr<IExporter> exporter);
	std::vector<std::shared_ptr<IExporter>> get_all() const;
	std::shared_ptr<IExporter> find_by_id(const std::string &id) const;
	std::shared_ptr<IExporter> find_by_extension(const std::string &ext) const;

	bool export_by_id(const std::string &id, const RecordingSession &session, const std::string &output_path) const;

private:
	ExporterRegistry();
	void register_defaults();

	std::vector<std::shared_ptr<IExporter>> exporters_;
};
