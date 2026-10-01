#pragma once

#include "exporter-base.hpp"
#include "recording-session.hpp"

#include <memory>
#include <vector>
#include <QButtonGroup>
#include <QDialog>
#include <QPushButton>

class ExportDialog : public QDialog {
	Q_OBJECT

public:
	explicit ExportDialog(const RecordingSession &session, QWidget *parent = nullptr);

private slots:
	void onExportClicked();
	void onCopyClicked();
	void onSelectionChanged(int id);

private:
	void setup_ui();

	const RecordingSession session_;
	QButtonGroup *btn_group_ = nullptr;
	QPushButton *btn_copy_ = nullptr;
	std::vector<std::shared_ptr<IExporter>> exporters_;
};
