#include "plugin-config.hpp"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QColor>

bool PluginConfig::load()
{
	QSettings settings(format_, QSettings::UserScope, "oz-coden", "obs-timestamp-memo");
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
	if (settings.status() != QSettings::NoError)
		return false;
	values_ = value;
	emit configChanged();
	return true;
}

bool PluginConfig::save(const PluginSettings &value)
{
	QSettings settings(format_, QSettings::UserScope, "oz-coden", "obs-timestamp-memo");

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
