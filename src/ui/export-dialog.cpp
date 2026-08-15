#include "export-dialog.hpp"
#include "csv-exporter.hpp"
#include "edl-exporter.hpp"
#include "json-exporter.hpp"
#include "srt-exporter.hpp"
#include "xml-exporter.hpp"

#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

ExportDialog::ExportDialog(const RecordingSession &session, QWidget *parent)
	: QDialog(parent), session_(session)
{
	setWindowTitle("Export Markers");
	setMinimumWidth(380);
	setup_ui();
}

void ExportDialog::setup_ui()
{
	auto *main_layout = new QVBoxLayout(this);

	auto *grp = new QGroupBox("Select Export Format", this);
	auto *vbox = new QVBoxLayout(grp);

	rb_csv_ = new QRadioButton("CSV (*.csv) - DaVinci Resolve / Premiere Pro", this);
	rb_edl_ = new QRadioButton("CMX 3600 EDL (*.edl) - Timecode Markers", this);
	rb_srt_ = new QRadioButton("SubRip Subtitle (*.srt) - Subtitles", this);
	rb_xml_ = new QRadioButton("Premiere Pro XML (*.xml) - Sequence Markers", this);
	rb_json_ = new QRadioButton("JSON (*.json) - Full Metadata & Markers", this);

	rb_csv_->setChecked(true);

	vbox->addWidget(rb_csv_);
	vbox->addWidget(rb_edl_);
	vbox->addWidget(rb_srt_);
	vbox->addWidget(rb_xml_);
	vbox->addWidget(rb_json_);
	main_layout->addWidget(grp);

	auto *btn_layout = new QHBoxLayout();
	btn_layout->addStretch();

	auto *btn_cancel = new QPushButton("Cancel", this);
	connect(btn_cancel, &QPushButton::clicked, this, &ExportDialog::reject);

	auto *btn_export = new QPushButton("Export...", this);
	btn_export->setDefault(true);
	connect(btn_export, &QPushButton::clicked, this, &ExportDialog::onExportClicked);

	btn_layout->addWidget(btn_cancel);
	btn_layout->addWidget(btn_export);
	main_layout->addLayout(btn_layout);
}

void ExportDialog::onExportClicked()
{
	std::unique_ptr<IExporter> exporter;
	QString filter;
	QString ext;

	if (rb_csv_->isChecked()) {
		exporter = std::make_unique<CsvExporter>();
		filter = "CSV File (*.csv)";
		ext = "csv";
	} else if (rb_edl_->isChecked()) {
		exporter = std::make_unique<EdlExporter>();
		filter = "CMX 3600 EDL (*.edl)";
		ext = "edl";
	} else if (rb_srt_->isChecked()) {
		exporter = std::make_unique<SrtExporter>();
		filter = "SRT Subtitle (*.srt)";
		ext = "srt";
	} else if (rb_xml_->isChecked()) {
		exporter = std::make_unique<XmlExporter>();
		filter = "Premiere XML (*.xml)";
		ext = "xml";
	} else {
		exporter = std::make_unique<JsonExporter>();
		filter = "JSON File (*.json)";
		ext = "json";
	}

	QString default_name = "markers." + ext;
	if (!session_.video_path().empty()) {
		QFileInfo fi(QString::fromStdString(session_.video_path()));
		default_name = fi.dir().filePath(fi.completeBaseName() + "." + ext);
	}

	QString save_path = QFileDialog::getSaveFileName(this, "Save Export File", default_name, filter);
	if (save_path.isEmpty()) {
		return;
	}

	if (exporter->export_to_file(session_, save_path.toStdString())) {
		QMessageBox::information(this, "Export Succeeded", QString("Successfully exported to:\n%1").arg(save_path));
		accept();
	} else {
		QMessageBox::critical(this, "Export Failed", QString("Failed to write to:\n%1").arg(save_path));
	}
}
