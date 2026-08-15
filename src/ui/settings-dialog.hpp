#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <vector>

class SettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent = nullptr);

signals:
	void settingsSaved();

private slots:
	void onPickColor(int index);
	void onSaveClicked();

private:
	void load_values();
	void setup_ui();

	struct MarkerUiSlot {
		QLineEdit *label_edit = nullptr;
		QPushButton *color_btn = nullptr;
		QString current_color;
	};

	std::vector<MarkerUiSlot> marker_slots_;

	QCheckBox *chk_auto_json_ = nullptr;
	QCheckBox *chk_auto_csv_ = nullptr;
	QCheckBox *chk_auto_edl_ = nullptr;
	QCheckBox *chk_auto_srt_ = nullptr;
	QCheckBox *chk_auto_xml_ = nullptr;

	QCheckBox *chk_status_bar_ = nullptr;
};
