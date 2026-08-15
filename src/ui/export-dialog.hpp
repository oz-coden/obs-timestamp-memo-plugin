#pragma once

#include "recording-session.hpp"
#include <QDialog>
#include <QRadioButton>
#include <QPushButton>

class ExportDialog : public QDialog {
	Q_OBJECT

public:
	explicit ExportDialog(const RecordingSession &session, QWidget *parent = nullptr);

private slots:
	void onExportClicked();

private:
	void setup_ui();

	const RecordingSession &session_;
	QRadioButton *rb_csv_ = nullptr;
	QRadioButton *rb_edl_ = nullptr;
	QRadioButton *rb_srt_ = nullptr;
	QRadioButton *rb_xml_ = nullptr;
	QRadioButton *rb_json_ = nullptr;
};
