#include "settings-dialog.hpp"
#include "plugin-config.hpp"

#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Timestamp Memo - Settings");
	setMinimumWidth(440);
	setup_ui();
	load_values();
}

void SettingsDialog::setup_ui()
{
	auto *main_layout = new QVBoxLayout(this);

	auto *grp_markers = new QGroupBox("Marker Configuration (4 Slots)", this);
	auto *markers_layout = new QVBoxLayout(grp_markers);

	marker_slots_.resize(4);
	for (int i = 0; i < 4; ++i) {
		auto *row_layout = new QHBoxLayout();
		auto *lbl = new QLabel(QString("Marker %1:").arg(i + 1), this);
		lbl->setFixedWidth(70);

		auto *edit = new QLineEdit(this);
		edit->setPlaceholderText(QString("Label for Marker %1").arg(i + 1));

		auto *color_btn = new QPushButton(this);
		color_btn->setFixedWidth(60);
		color_btn->setText("Color");

		connect(color_btn, &QPushButton::clicked, this, [this, i]() { onPickColor(i); });

		row_layout->addWidget(lbl);
		row_layout->addWidget(edit);
		row_layout->addWidget(color_btn);

		markers_layout->addLayout(row_layout);

		marker_slots_[i].label_edit = edit;
		marker_slots_[i].color_btn = color_btn;
	}
	main_layout->addWidget(grp_markers);

	auto *grp_auto = new QGroupBox("Auto-Export on Recording Stop", this);
	auto *auto_layout = new QVBoxLayout(grp_auto);

	chk_auto_json_ = new QCheckBox("Export JSON (*.json) - Recommended", this);
	chk_auto_csv_ = new QCheckBox("Export CSV (*.csv) - DaVinci / Premiere", this);
	chk_auto_edl_ = new QCheckBox("Export CMX 3600 EDL (*.edl) - Timecode Markers", this);
	chk_auto_srt_ = new QCheckBox("Export SubRip Subtitle (*.srt) - Subtitles", this);
	chk_auto_xml_ = new QCheckBox("Export Premiere Pro XML (*.xml) - Sequence Markers", this);

	auto_layout->addWidget(chk_auto_json_);
	auto_layout->addWidget(chk_auto_csv_);
	auto_layout->addWidget(chk_auto_edl_);
	auto_layout->addWidget(chk_auto_srt_);
	auto_layout->addWidget(chk_auto_xml_);
	main_layout->addWidget(grp_auto);

	auto *grp_general = new QGroupBox("General Options", this);
	auto *gen_layout = new QVBoxLayout(grp_general);
	chk_status_bar_ = new QCheckBox("Show notification in OBS status bar upon stamping", this);
	gen_layout->addWidget(chk_status_bar_);
	main_layout->addWidget(grp_general);

	auto *btn_layout = new QHBoxLayout();
	btn_layout->addStretch();

	auto *btn_save = new QPushButton("Save Settings", this);
	btn_save->setDefault(true);
	connect(btn_save, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);

	auto *btn_cancel = new QPushButton("Cancel", this);
	connect(btn_cancel, &QPushButton::clicked, this, &SettingsDialog::reject);

	btn_layout->addWidget(btn_cancel);
	btn_layout->addWidget(btn_save);
	main_layout->addLayout(btn_layout);
}

void SettingsDialog::load_values()
{
	const auto &cfg = PluginConfig::instance();

	for (int i = 0; i < 4 && i < static_cast<int>(cfg.marker_types.size()); ++i) {
		marker_slots_[i].label_edit->setText(QString::fromStdString(cfg.marker_types[i].label));
		marker_slots_[i].current_color = QString::fromStdString(cfg.marker_types[i].color);
		marker_slots_[i].color_btn->setStyleSheet(
			QString("background-color: %1; color: %2; font-weight: bold; border-radius: 3px;")
				.arg(marker_slots_[i].current_color)
				.arg(QColor(marker_slots_[i].current_color).lightness() > 130 ? "#000000" : "#ffffff"));
	}

	chk_auto_json_->setChecked(cfg.auto_export.json);
	chk_auto_csv_->setChecked(cfg.auto_export.csv);
	chk_auto_edl_->setChecked(cfg.auto_export.edl);
	chk_auto_srt_->setChecked(cfg.auto_export.srt);
	chk_auto_xml_->setChecked(cfg.auto_export.xml);

	chk_status_bar_->setChecked(cfg.show_status_bar_notification);
}

void SettingsDialog::onPickColor(int index)
{
	if (index < 0 || index >= static_cast<int>(marker_slots_.size()))
		return;

	QColor initial(marker_slots_[index].current_color);
	QColor color = QColorDialog::getColor(initial, this, QString("Select Color for Marker %1").arg(index + 1));
	if (color.isValid()) {
		marker_slots_[index].current_color = color.name();
		marker_slots_[index].color_btn->setStyleSheet(
			QString("background-color: %1; color: %2; font-weight: bold; border-radius: 3px;")
				.arg(color.name())
				.arg(color.lightness() > 130 ? "#000000" : "#ffffff"));
	}
}

void SettingsDialog::onSaveClicked()
{
	auto &cfg = PluginConfig::instance();

	for (int i = 0; i < 4 && i < static_cast<int>(marker_slots_.size()); ++i) {
		QString label = marker_slots_[i].label_edit->text().trimmed();
		if (label.isEmpty()) {
			label = QString("Marker %1").arg(i + 1);
		}
		cfg.marker_types[i].label = label.toStdString();
		cfg.marker_types[i].color = marker_slots_[i].current_color.toStdString();
	}

	cfg.auto_export.json = chk_auto_json_->isChecked();
	cfg.auto_export.csv = chk_auto_csv_->isChecked();
	cfg.auto_export.edl = chk_auto_edl_->isChecked();
	cfg.auto_export.srt = chk_auto_srt_->isChecked();
	cfg.auto_export.xml = chk_auto_xml_->isChecked();

	cfg.show_status_bar_notification = chk_status_bar_->isChecked();

	cfg.save();

	emit settingsSaved();
	accept();
}
