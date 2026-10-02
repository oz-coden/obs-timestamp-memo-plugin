#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <vector>

class PluginConfig;

class SettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit SettingsDialog(PluginConfig &config, QWidget *parent = nullptr);

private slots:
	void onPickColor(int index);
	void onSaveClicked();
	void onResetDefaultsClicked();

private:
	PluginConfig &config_;
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
	QCheckBox *chk_auto_vtt_ = nullptr;
	QCheckBox *chk_auto_youtube_ = nullptr;
	QCheckBox *chk_auto_markdown_ = nullptr;
	QCheckBox *chk_auto_xml_ = nullptr;

	QCheckBox *chk_status_bar_ = nullptr;
};
