#include "export-dialog.hpp"
#include "exporter-registry.hpp"

#include <QButtonGroup>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QPointer>

ExportDialog::ExportDialog(const RecordingSession &session, const ExporterRegistry &exporters, QWidget *parent)
	: QDialog(parent)
{
	set_snapshot(session);
	setMinimumWidth(380);

	exporters_ = exporters.get_all();
	setup_ui();
}

void ExportDialog::set_snapshot(const RecordingSession &session)
{
	session_ = std::make_shared<const RecordingSession>(session);
	setWindowTitle(tr("Export Markers — %1 (%2 markers)")
			       .arg(QFileInfo(QString::fromStdString(session.video_path())).fileName())
			       .arg(session.get_markers().size()));
}

void ExportDialog::setup_ui()
{
	auto *main_layout = new QVBoxLayout(this);

	auto *grp = new QGroupBox(tr("Select Export Format"), this);
	auto *vbox = new QVBoxLayout(grp);

	btn_group_ = new QButtonGroup(this);

	for (size_t i = 0; i < exporters_.size(); ++i) {
		const auto &exp = exporters_[i];
		QString text =
			tr("%1 (*.%2)")
				.arg(QCoreApplication::translate("TimestampMemo", exp->get_format_name().c_str()),
				     QString::fromStdString(exp->get_file_extension()));
		auto *rb = new QRadioButton(text, this);
		if (i == 0) {
			rb->setChecked(true);
		}
		btn_group_->addButton(rb, static_cast<int>(i));
		vbox->addWidget(rb);
	}

	connect(btn_group_, &QButtonGroup::idClicked, this, &ExportDialog::onSelectionChanged);

	main_layout->addWidget(grp);

	auto *btn_layout = new QHBoxLayout();

	btn_copy_ = new QPushButton(tr("Copy to Clipboard"), this);
	connect(btn_copy_, &QPushButton::clicked, this, &ExportDialog::onCopyClicked);
	btn_layout->addWidget(btn_copy_);

	btn_layout->addStretch();

	auto *btn_cancel = new QPushButton(tr("Cancel"), this);
	connect(btn_cancel, &QPushButton::clicked, this, &ExportDialog::reject);

	auto *btn_export = new QPushButton(tr("Export..."), this);
	btn_export->setDefault(true);
	connect(btn_export, &QPushButton::clicked, this, &ExportDialog::onExportClicked);

	btn_layout->addWidget(btn_cancel);
	btn_layout->addWidget(btn_export);
	main_layout->addLayout(btn_layout);

	// Update copy button initial state
	if (!exporters_.empty()) {
		onSelectionChanged(btn_group_->checkedId());
	}
}

void ExportDialog::onSelectionChanged(int id)
{
	if (id >= 0 && id < static_cast<int>(exporters_.size())) {
		btn_copy_->setEnabled(exporters_[id]->can_export_to_string());
	} else {
		btn_copy_->setEnabled(false);
	}
}

void ExportDialog::onCopyClicked()
{
	int selected_id = btn_group_->checkedId();
	if (selected_id < 0 || selected_id >= static_cast<int>(exporters_.size())) {
		return;
	}

	const auto &exporter = exporters_[selected_id];
	if (!exporter->can_export_to_string()) {
		return;
	}

	const auto snapshot = session_;
	std::string text = exporter->export_to_string(*snapshot);
	if (!QGuiApplication::clipboard()) {
		QMessageBox::critical(this, tr("Copy Failed"), tr("The system clipboard is unavailable."));
		return;
	}
	QGuiApplication::clipboard()->setText(QString::fromStdString(text));
}

void ExportDialog::onExportClicked()
{
	QPointer<ExportDialog> guard(this);
	int selected_id = btn_group_->checkedId();
	if (selected_id < 0 || selected_id >= static_cast<int>(exporters_.size())) {
		return;
	}

	// A nested file dialog can process document edits, splits and refreshes.
	// Keep this operation's immutable snapshot and exporter alive independently.
	const auto snapshot = session_;
	const auto exporter = exporters_[selected_id];
	QString ext = QString::fromStdString(exporter->get_file_extension());
	QString filter =
		QString("%1 (*.%2)")
			.arg(QCoreApplication::translate("TimestampMemo", exporter->get_format_name().c_str()), ext);

	QString default_name = "markers." + ext;
	if (!snapshot->video_path().empty()) {
		QFileInfo fi(QString::fromStdString(snapshot->video_path()));
		default_name = fi.dir().filePath(fi.completeBaseName() + "." + ext);
	}

	QString save_path = QFileDialog::getSaveFileName(this, tr("Save Export File"), default_name, filter);
	if (!guard || save_path.isEmpty()) {
		return;
	}

	if (exporter->export_to_file(*snapshot, save_path.toStdString())) {
		if (session_ == snapshot)
			accept();
	} else {
		QMessageBox::critical(this, tr("Export Failed"), tr("Failed to write to:\n%1").arg(save_path));
	}
}
