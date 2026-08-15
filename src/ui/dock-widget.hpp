#pragma once

#include "marker-table-model.hpp"
#include "recording-session.hpp"

#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <vector>

class DockWidget : public QDockWidget {
	Q_OBJECT

public:
	explicit DockWidget(QWidget *parent = nullptr);
	~DockWidget();

public slots:
	void refreshMarkerButtons();

private slots:
	void onQuickMarkerClicked(int index);
	void onAddMemoClicked();
	void onOpenJsonClicked();
	void onExportClicked();
	void onSettingsClicked();
	void onClearClicked();
	void onMarkerCommentChanged(uint32_t marker_id, const QString &comment);
	void onContextMenuRequested(const QPoint &pos);
	void updateLiveTimer();

	// ObsBridge シグナルハンドラ
	void onMarkerAdded(const MemoMarker &marker);
	void onRecordingStarted(const QString &videoPath);
	void onRecordingPaused();
	void onRecordingUnpaused();
	void onRecordingStopped(const QString &jsonPath);
	void onRecordingFileChanged(const QString &newVideoPath);
	void onFocusMemoInputRequested();

private:
	void setup_ui();
	void update_status_ui();

	QLabel *lbl_status_ = nullptr;
	QLabel *lbl_video_name_ = nullptr;
	QLabel *lbl_live_time_ = nullptr;

	QLineEdit *edit_memo_ = nullptr;
	QPushButton *btn_add_memo_ = nullptr;

	std::vector<QPushButton *> quick_marker_btns_;

	QTableView *table_view_ = nullptr;
	MarkerTableModel *table_model_ = nullptr;

	QPushButton *btn_open_ = nullptr;
	QPushButton *btn_export_ = nullptr;
	QPushButton *btn_clear_ = nullptr;
	QPushButton *btn_settings_ = nullptr;

	QTimer *live_timer_ = nullptr;
};
