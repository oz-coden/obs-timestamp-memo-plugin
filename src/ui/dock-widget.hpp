#pragma once

#include "marker-table-model.hpp"

#include <vector>
#include <QWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTimer>

// Content widget. obs_frontend_add_dock_by_id owns the outer QDockWidget.
class DockWidget : public QWidget {
	Q_OBJECT

public:
	explicit DockWidget(QWidget *parent = nullptr);
	~DockWidget();

public slots:
	void refreshMarkerButtons();

protected:
	void keyPressEvent(QKeyEvent *event) override;

private slots:
	void onQuickMarkerClicked(int index);
	void onAddMemoClicked();
	void onOpenJsonClicked();
	void onExportClicked();
	void onSettingsClicked();
	void onClearClicked();
	void onMarkerDataChanged(uint32_t marker_id, const QString &label, const QString &comment);
	void onContextMenuRequested(const QPoint &pos);
	void updateLiveTimer();

	void onMarkerAdded(const MemoMarker &marker, int row);
	void onMarkerUpdated(const MemoMarker &marker, int row);
	void onMarkerRemoved(uint32_t id, int row);
	void onMarkersReset(const std::vector<MemoMarker> &markers, const VideoFrameRate &fps);

	void onSessionStarted(const QString &videoPath);
	void onSessionPaused();
	void onSessionResumed();
	void onSessionStopped(const QString &jsonPath);
	void onVideoPathResolved(const QString &newVideoPath);
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
