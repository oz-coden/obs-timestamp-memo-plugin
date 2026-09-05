#include "export-dialog.hpp"
#include "exporter-registry.hpp"

#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QRadioButton>
#include <QVBoxLayout>

ExportDialog::ExportDialog(const RecordingSession &session, QWidget *parent) : QDialog(parent), session_(session)
{
	setWindowTitle("Export Markers");
	setMinimumWidth(380);

	exporters_ = ExporterRegistry::instance().get_all();
	setup_ui();
}

void ExportDialog::setup_ui()
{
	auto *main_layout = new QVBoxLayout(this);

	auto *grp = new QGroupBox("Select Export Format", this);
	auto *vbox = new QVBoxLayout(grp);

	btn_group_ = new QButtonGroup(this);

	for (size_t i = 0; i < exporters_.size(); ++i) {
		const auto &exp = exporters_[i];
		QString text = QString::fromStdString(exp->get_filter_string());
		auto *rb = new QRadioButton(text, this);
		if (i == 0) {
			rb->setChecked(true);
		}
		btn_group_->addButton(rb, static_cast<int>(i));
		vbox->addWidget(rb);
	}

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
	int selected_id = btn_group_->checkedId();
	if (selected_id < 0 || selected_id >= static_cast<int>(exporters_.size())) {
		return;
	}

	const auto &exporter = exporters_[selected_id];
	QString ext = QString::fromStdString(exporter->get_file_extension());
	QString filter = QString("%1 (*.%2)").arg(QString::fromStdString(exporter->get_format_name()), ext);

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
		QMessageBox::information(this, "Export Succeeded",
					 QString("Successfully exported to:\n%1").arg(save_path));
		accept();
	} else {
		QMessageBox::critical(this, "Export Failed", QString("Failed to write to:\n%1").arg(save_path));
	}
}
