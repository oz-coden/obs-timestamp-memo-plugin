#include "plugin-config.hpp"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QColor>

PluginConfig &PluginConfig::instance()
{
	static PluginConfig cfg;
	return cfg;
}

PluginConfig::PluginConfig()
{
	set_defaults();
	load();
}

void PluginConfig::set_defaults()
{
	marker_types.clear();
	marker_types.push_back({0, "Marker 1", "#3498db", "hotkey_marker_1"});
	marker_types.push_back({1, "Chapter", "#2ecc71", "hotkey_marker_2"});
	marker_types.push_back({2, "Highlight", "#f1c40f", "hotkey_marker_3"});
	marker_types.push_back({3, "Cut / Edit", "#e74c3c", "hotkey_marker_4"});

	auto_export.json = true;
	auto_export.csv = false;
	auto_export.edl = false;
	auto_export.srt = false;
	auto_export.vtt = false;
	auto_export.youtube = false;
	auto_export.markdown = false;
	auto_export.xml = false;

	show_status_bar_notification = true;
	keep_tmp_cache_on_crash = true;
}

void PluginConfig::load()
{
	QSettings settings("oz-coden", "obs-timestamp-memo");

	settings.beginGroup("Markers");
	for (int i = 0; i < 4; ++i) {
		QString prefix = QString("marker_%1_").arg(i + 1);
		QString label =
			settings.value(prefix + "label", QString::fromStdString(marker_types[i].label)).toString();
		QString color =
			settings.value(prefix + "color", QString::fromStdString(marker_types[i].color)).toString();
		marker_types[i].label = label.toStdString();
		if (QColor(color).isValid())
			marker_types[i].color = QColor(color).name().toStdString();
	}
	settings.endGroup();

	settings.beginGroup("AutoExport");
	auto_export.json = settings.value("json", true).toBool();
	auto_export.csv = settings.value("csv", false).toBool();
	auto_export.edl = settings.value("edl", false).toBool();
	auto_export.srt = settings.value("srt", false).toBool();
	auto_export.vtt = settings.value("vtt", false).toBool();
	auto_export.youtube = settings.value("youtube", false).toBool();
	auto_export.markdown = settings.value("markdown", false).toBool();
	auto_export.xml = settings.value("xml", false).toBool();
	settings.endGroup();

	settings.beginGroup("General");
	show_status_bar_notification = settings.value("show_status_bar_notification", true).toBool();
	keep_tmp_cache_on_crash = settings.value("keep_tmp_cache_on_crash", true).toBool();
	settings.endGroup();
}

bool PluginConfig::save()
{
	QSettings settings("oz-coden", "obs-timestamp-memo");

	settings.beginGroup("Markers");
	for (int i = 0; i < 4; ++i) {
		QString prefix = QString("marker_%1_").arg(i + 1);
		settings.setValue(prefix + "label", QString::fromStdString(marker_types[i].label));
		settings.setValue(prefix + "color", QString::fromStdString(marker_types[i].color));
	}
	settings.endGroup();

	settings.beginGroup("AutoExport");
	settings.setValue("json", auto_export.json);
	settings.setValue("csv", auto_export.csv);
	settings.setValue("edl", auto_export.edl);
	settings.setValue("srt", auto_export.srt);
	settings.setValue("vtt", auto_export.vtt);
	settings.setValue("youtube", auto_export.youtube);
	settings.setValue("markdown", auto_export.markdown);
	settings.setValue("xml", auto_export.xml);
	settings.endGroup();

	settings.beginGroup("General");
	settings.setValue("show_status_bar_notification", show_status_bar_notification);
	settings.setValue("keep_tmp_cache_on_crash", keep_tmp_cache_on_crash);
	settings.endGroup();

	settings.sync();
	emit configChanged();
	return settings.status() == QSettings::NoError;
}
