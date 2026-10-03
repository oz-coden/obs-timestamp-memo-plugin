#include "plugin-config.hpp"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QColor>
#include <QFileInfo>
#include <memory>

namespace {
PluginSettings read_settings(QSettings &settings)
{
	PluginSettings value;

	settings.beginGroup("Markers");
	for (int i = 0; i < 4; ++i) {
		QString prefix = QString("marker_%1_").arg(i + 1);
		QString label =
			settings.value(prefix + "label", QString::fromStdString(value.marker_types[i].label)).toString();
		QString color =
			settings.value(prefix + "color", QString::fromStdString(value.marker_types[i].color)).toString();
		value.marker_types[i].label = label.toStdString();
		if (QColor(color).isValid())
			value.marker_types[i].color = QColor(color).name().toStdString();
	}
	settings.endGroup();

	settings.beginGroup("AutoExport");
	value.auto_export.json = settings.value("json", true).toBool();
	value.auto_export.csv = settings.value("csv", false).toBool();
	value.auto_export.edl = settings.value("edl", false).toBool();
	value.auto_export.srt = settings.value("srt", false).toBool();
	value.auto_export.vtt = settings.value("vtt", false).toBool();
	value.auto_export.youtube = settings.value("youtube", false).toBool();
	value.auto_export.markdown = settings.value("markdown", false).toBool();
	value.auto_export.xml = settings.value("xml", false).toBool();
	settings.endGroup();

	settings.beginGroup("General");
	value.show_status_bar_notification = settings.value("show_status_bar_notification", true).toBool();
	settings.endGroup();
	return value;
}
} // namespace

bool PluginConfig::load()
{
	if (file_path_) {
		if (file_path_->empty())
			return false;
		const auto path = QString::fromStdString(*file_path_);
		if (!QFileInfo::exists(path)) {
			QSettings legacy(format_, QSettings::UserScope, "oz-coden", "obs-timestamp-memo");
			auto migrated = read_settings(legacy);
			if (legacy.status() != QSettings::NoError)
				return false;
			return save(migrated); // Commit migration before changing active settings; keep legacy data.
		}
		QSettings current(path, QSettings::IniFormat);
		if (current.value("SchemaVersion", 1).toInt() != 1)
			return false;
		auto candidate = read_settings(current);
		if (current.status() != QSettings::NoError)
			return false;
		values_ = std::move(candidate);
	} else {
		QSettings legacy(format_, QSettings::UserScope, "oz-coden", "obs-timestamp-memo");
		auto candidate = read_settings(legacy);
		if (legacy.status() != QSettings::NoError)
			return false;
		values_ = std::move(candidate);
	}
	emit configChanged();
	return true;
}

bool PluginConfig::save(const PluginSettings &value)
{
	std::unique_ptr<QSettings> owned;
	if (file_path_) {
		if (file_path_->empty())
			return false;
		QFileInfo file(QString::fromStdString(*file_path_));
		if (!file.dir().mkpath("."))
			return false;
		owned = std::make_unique<QSettings>(file.filePath(), QSettings::IniFormat);
	} else {
		owned = std::make_unique<QSettings>(format_, QSettings::UserScope, "oz-coden", "obs-timestamp-memo");
	}
	auto &settings = *owned;
	settings.setAtomicSyncRequired(true);
	settings.setValue("SchemaVersion", 1);

	settings.beginGroup("Markers");
	for (int i = 0; i < 4; ++i) {
		QString prefix = QString("marker_%1_").arg(i + 1);
		settings.setValue(prefix + "label", QString::fromStdString(value.marker_types[i].label));
		settings.setValue(prefix + "color", QString::fromStdString(value.marker_types[i].color));
	}
	settings.endGroup();

	settings.beginGroup("AutoExport");
	settings.setValue("json", value.auto_export.json);
	settings.setValue("csv", value.auto_export.csv);
	settings.setValue("edl", value.auto_export.edl);
	settings.setValue("srt", value.auto_export.srt);
	settings.setValue("vtt", value.auto_export.vtt);
	settings.setValue("youtube", value.auto_export.youtube);
	settings.setValue("markdown", value.auto_export.markdown);
	settings.setValue("xml", value.auto_export.xml);
	settings.endGroup();

	settings.beginGroup("General");
	settings.setValue("show_status_bar_notification", value.show_status_bar_notification);
	settings.endGroup();

	settings.sync();
	if (settings.status() != QSettings::NoError)
		return false;
	values_ = value;
	emit configChanged();
	return true;
}
