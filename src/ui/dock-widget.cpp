#include "dock-widget.hpp"
#include "export-dialog.hpp"
#include "plugin-config.hpp"
#include "session-controller.hpp"
#include "settings-dialog.hpp"
#include "timecode-helper.hpp"
#include "youtube-exporter.hpp"

#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QDockWidget>

DockWidget::DockWidget(SessionController &controller, PluginConfig &config, ExporterRegistry &exporters,
		       QWidget *parent)
	: QWidget(parent),
	  controller_(controller),
	  config_(config),
	  exporters_(exporters)
{
	setObjectName("ObsTimestampMemoDock");
	setWindowTitle("Timestamp Memo & Markers");

	setup_ui();

	connect(&controller, &SessionController::markerAdded, this, &DockWidget::onMarkerAdded);
	connect(&controller, &SessionController::markerUpdated, this, &DockWidget::onMarkerUpdated);
	connect(&controller, &SessionController::markerRemoved, this, &DockWidget::onMarkerRemoved);
	connect(&controller, &SessionController::markersReset, this, &DockWidget::onMarkersReset);

	connect(&controller, &SessionController::sessionStarted, this, &DockWidget::onSessionStarted);
	connect(&controller, &SessionController::sessionPaused, this, &DockWidget::onSessionPaused);
	connect(&controller, &SessionController::sessionResumed, this, &DockWidget::onSessionResumed);
	connect(&controller, &SessionController::sessionStopped, this, &DockWidget::onSessionStopped);
	connect(&controller, &SessionController::videoPathResolved, this, &DockWidget::onVideoPathResolved);
	connect(&controller, &SessionController::focusMemoInputRequested, this, &DockWidget::onFocusMemoInputRequested);

	connect(&config_, &PluginConfig::configChanged, this, [this]() {
		refreshMarkerButtons();
		table_view_->viewport()->update();
	});

	connect(table_model_, &MarkerTableModel::markerDataChanged, this, &DockWidget::onMarkerDataChanged);

	live_timer_ = new QTimer(this);
	connect(live_timer_, &QTimer::timeout, this, &DockWidget::updateLiveTimer);
	live_timer_->start(250);

	update_status_ui();
	refreshMarkerButtons();
	table_model_->set_markers(controller.session().get_markers(), controller.session().frame_rate());
}

DockWidget::~DockWidget() {}

void DockWidget::setup_ui()
{
	auto *container = this;
	auto *main_layout = new QVBoxLayout(container);
	main_layout->setContentsMargins(6, 6, 6, 6);
	main_layout->setSpacing(6);

	auto *status_panel = new QFrame(container);
	status_panel->setFrameShape(QFrame::StyledPanel);
	status_panel->setStyleSheet("background: palette(alternate-base); border-radius: 4px; padding: 2px;");

	auto *status_layout = new QHBoxLayout(status_panel);
	status_layout->setContentsMargins(6, 4, 6, 4);

	lbl_status_ = new QLabel("■ STOPPED", status_panel);
	lbl_status_->setStyleSheet("font-weight: bold; color: #888888;");

	lbl_live_time_ = new QLabel("00:00:00.000", status_panel);
	lbl_live_time_->setStyleSheet("font-family: monospace; font-weight: bold;");

	lbl_video_name_ = new QLabel("No active recording", status_panel);
	lbl_video_name_->setStyleSheet("color: #aaaaaa; font-size: 11px;");
	lbl_video_name_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	status_layout->addWidget(lbl_status_);
	status_layout->addWidget(lbl_live_time_);
	status_layout->addStretch();
	status_layout->addWidget(lbl_video_name_);
	main_layout->addWidget(status_panel);

	auto *quick_layout = new QHBoxLayout();
	quick_layout->setSpacing(4);
	quick_marker_btns_.resize(4);

	for (int i = 0; i < 4; ++i) {
		auto *btn = new QPushButton(QString("Marker %1").arg(i + 1), container);
		btn->setCursor(Qt::PointingHandCursor);
		btn->setFixedHeight(28);
		btn->setStyleSheet("font-weight: bold; border-radius: 3px; padding: 2px 6px;");

		connect(btn, &QPushButton::clicked, this, [this, i]() { onQuickMarkerClicked(i); });

		quick_layout->addWidget(btn);
		quick_marker_btns_[i] = btn;
	}
	main_layout->addLayout(quick_layout);

	auto *memo_layout = new QHBoxLayout();
	memo_layout->setSpacing(4);

	edit_memo_ = new QLineEdit(container);
	edit_memo_->setPlaceholderText("Write memo & press Enter to stamp...");
	edit_memo_->setFixedHeight(28);
	connect(edit_memo_, &QLineEdit::returnPressed, this, &DockWidget::onAddMemoClicked);

	btn_add_memo_ = new QPushButton("Stamp Memo", container);
	btn_add_memo_->setFixedHeight(28);
	btn_add_memo_->setCursor(Qt::PointingHandCursor);
	connect(btn_add_memo_, &QPushButton::clicked, this, &DockWidget::onAddMemoClicked);

	memo_layout->addWidget(edit_memo_);
	memo_layout->addWidget(btn_add_memo_);
	main_layout->addLayout(memo_layout);

	table_view_ = new QTableView(container);
	table_model_ = new MarkerTableModel(this);
	table_view_->setModel(table_model_);
	table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
	table_view_->setAlternatingRowColors(true);
	table_view_->verticalHeader()->setVisible(false);
	table_view_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(table_view_, &QTableView::customContextMenuRequested, this, &DockWidget::onContextMenuRequested);

	auto *header = table_view_->horizontalHeader();
	header->setSectionResizeMode(MarkerTableModel::Col_Id, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(MarkerTableModel::Col_Timecode, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(MarkerTableModel::Col_Frame, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(MarkerTableModel::Col_Label, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(MarkerTableModel::Col_Comment, QHeaderView::Stretch);
	header->setSectionResizeMode(MarkerTableModel::Col_Status, QHeaderView::ResizeToContents);

	main_layout->addWidget(table_view_);

	auto *bottom_layout = new QHBoxLayout();
	bottom_layout->setSpacing(4);

	btn_open_ = new QPushButton("Open JSON...", container);
	btn_open_->setFixedHeight(26);
	connect(btn_open_, &QPushButton::clicked, this, &DockWidget::onOpenJsonClicked);

	btn_export_ = new QPushButton("Export...", container);
	btn_export_->setFixedHeight(26);
	connect(btn_export_, &QPushButton::clicked, this, &DockWidget::onExportClicked);

	btn_clear_ = new QPushButton("Clear", container);
	btn_clear_->setFixedHeight(26);
	connect(btn_clear_, &QPushButton::clicked, this, &DockWidget::onClearClicked);

	btn_settings_ = new QPushButton("Settings", container);
	btn_settings_->setFixedHeight(26);
	connect(btn_settings_, &QPushButton::clicked, this, &DockWidget::onSettingsClicked);

	bottom_layout->addWidget(btn_open_);
	bottom_layout->addWidget(btn_export_);
	bottom_layout->addWidget(btn_clear_);
	bottom_layout->addStretch();
	bottom_layout->addWidget(btn_settings_);
	main_layout->addLayout(bottom_layout);
}

void DockWidget::keyPressEvent(QKeyEvent *event)
{
	if (table_view_->hasFocus() && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
		QModelIndex idx = table_view_->currentIndex();
		if (idx.isValid()) {
			int row = idx.row();
			const auto &markers = table_model_->get_markers();
			if (row >= 0 && row < static_cast<int>(markers.size())) {
				uint32_t mid = markers[row].id;
				controller_.delete_marker(mid);
				event->accept();
				return;
			}
		}
	}
	QWidget::keyPressEvent(event);
}

void DockWidget::refreshMarkerButtons()
{
	const auto &cfg = config_.values();
	for (int i = 0; i < 4 && i < static_cast<int>(cfg.marker_types.size()); ++i) {
		QString label = QString::fromStdString(cfg.marker_types[i].label);
		QString color = QString::fromStdString(cfg.marker_types[i].color);
		quick_marker_btns_[i]->setText(QString("%1: %2").arg(i + 1).arg(label));
		quick_marker_btns_[i]->setStyleSheet(
			QString("background-color: %1; color: %2; font-weight: bold; border-radius: 3px; padding: 2px "
				"6px;")
				.arg(color)
				.arg(QColor(color).lightness() > 130 ? "#000000" : "#ffffff"));
	}
}

void DockWidget::update_status_ui()
{
	auto &controller = controller_;
	bool rec = controller.is_recording();

	btn_open_->setEnabled(!rec);

	for (auto *button : quick_marker_btns_)
		button->setEnabled(rec);
	btn_add_memo_->setEnabled(rec);
	bool has_markers = !controller.session().get_markers().empty();
	if (!rec)
		lbl_live_time_->setText("—");
	btn_export_->setEnabled(has_markers);
	btn_clear_->setEnabled(has_markers);
	lbl_status_->setText(rec ? (controller.is_paused() ? "⏸ PAUSED" : "● REC") : "■ STOPPED");
	lbl_status_->setStyleSheet(QString("font-weight:bold;color:%1;")
					   .arg(rec ? (controller.is_paused() ? "#f39c12" : "#e74c3c") : "#888888"));
	lbl_video_name_->setText(controller.document_title());
}

void DockWidget::updateLiveTimer()
{
	auto &controller = controller_;
	if (controller.is_recording()) {

		uint64_t ms = controller.current_record_ms();
		lbl_live_time_->setText(QString::fromStdString(TimecodeHelper::ms_to_timestamp_str(ms, true)));
	}
}

void DockWidget::onQuickMarkerClicked(int index)
{
	controller_.trigger_quick_marker(index);
}

void DockWidget::onAddMemoClicked()
{
	QString text = edit_memo_->text().trimmed();
	if (text.isEmpty()) {
		return;
	}

	if (controller_.add_memo_marker(text.toStdString(), 0)) {
		edit_memo_->clear();
	}
}

void DockWidget::onOpenJsonClicked()
{
	QPointer<DockWidget> guard(this);
	if (controller_.is_recording()) {
		QMessageBox::warning(this, "Action Blocked", "Cannot open another file while recording is active.");
		return;
	}

	QString initial_dir = "";
	std::string current_path = controller_.current_video_path();
	if (!current_path.empty()) {
		QFileInfo fi(QString::fromStdString(current_path));
		initial_dir = fi.dir().absolutePath();
	}

	QString path = QFileDialog::getOpenFileName(
		this, "Open Recording JSON or Cache", initial_dir,
		"JSON / Cache Files (*.json *.tmp.jsonl);;JSON Files (*.json);;Cache Files (*.tmp.jsonl);;All Files "
		"(*.*)");
	if (!guard || path.isEmpty()) {
		return;
	}

	auto &controller = controller_;
	bool loaded = false;

	if (path.endsWith(".tmp.jsonl", Qt::CaseInsensitive)) {
		loaded = controller.recover_from_cache(path.toStdString());
		if (loaded) {
			update_status_ui();
			controller_.notify("Recovered " + std::to_string(controller.session().get_markers().size()) +
						   " markers from cache",
					   3000);
		}
	} else {
		loaded = controller.load_from_json(path.toStdString());
		if (loaded) {
			update_status_ui();
			controller_.notify("Loaded " + std::to_string(controller.session().get_markers().size()) +
						   " markers from JSON",
					   2000);
		}
	}

	if (!loaded) {
		QMessageBox::warning(this, "Open Error", "Failed to parse recording JSON or cache file.");
	}
}

void DockWidget::onExportClicked()
{
	const auto &session = controller_.session();
	if (session.get_markers().empty()) {
		controller_.notify("There are no markers to export.", 2000);
		return;
	}

	if (export_dialog_ && export_dialog_->session_id() != session.session_id()) {
		export_dialog_->close();
		export_dialog_.clear();
	}
	if (!export_dialog_) {
		export_dialog_ = new ExportDialog(session, exporters_, this);
		export_dialog_->setAttribute(Qt::WA_DeleteOnClose);
	}
	export_dialog_->show();
	export_dialog_->raise();
	export_dialog_->activateWindow();
}

void DockWidget::onSettingsClicked()
{
	if (!settings_dialog_) {
		settings_dialog_ = new SettingsDialog(config_, this);
		settings_dialog_->setAttribute(Qt::WA_DeleteOnClose);
	}
	settings_dialog_->show();
	settings_dialog_->raise();
	settings_dialog_->activateWindow();
}

void DockWidget::onClearClicked()
{
	if (table_model_->get_markers().empty())
		return;

	const auto session_id = controller_.session().session_id();
	QPointer<DockWidget> guard(this);
	auto btn = QMessageBox::question(this, "Clear Markers", "Are you sure you want to clear all markers?",
					 QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (guard && btn == QMessageBox::Yes && controller_.session().session_id() == session_id) {
		controller_.clear_markers();
	}
}

void DockWidget::onMarkerDataChanged(uint32_t marker_id, const QString &label, const QString &comment)
{
	controller_.update_marker_data(marker_id, label.toStdString(), "", comment.toStdString());
}

void DockWidget::onContextMenuRequested(const QPoint &pos)
{
	QModelIndex idx = table_view_->indexAt(pos);
	if (!idx.isValid())
		return;

	int row = idx.row();
	const auto &markers = table_model_->get_markers();
	if (row < 0 || row >= static_cast<int>(markers.size()))
		return;

	// QMenu::exec processes recording/hotkey events that can reset the model.
	const auto marker = markers[row];
	auto &controller = controller_;
	const auto fps = controller.session().frame_rate();
	const auto session_id = controller.session().session_id();
	const auto session_snapshot = controller.session();

	QPointer<DockWidget> guard(this);
	QMenu menu;

	QAction *act_copy_tc = menu.addAction("Copy Timecode");
	QAction *act_copy_memo = menu.addAction("Copy Memo Text");
	menu.addSeparator();
	QAction *act_copy_yt = menu.addAction("Copy YouTube Chapters (All)");
	menu.addSeparator();

	QMenu *menu_type = menu.addMenu("Change Type");
	const auto &cfg = config_.values();
	for (int i = 0; i < 4 && i < static_cast<int>(cfg.marker_types.size()); ++i) {
		QString label = QString::fromStdString(cfg.marker_types[i].label);
		QAction *act_type = menu_type->addAction(QString("%1: %2").arg(i + 1).arg(label));
		connect(act_type, &QAction::triggered, this, [this, marker_id = marker.id, session_id, i]() {
			auto &current = controller_;
			if (current.session().session_id() == session_id)
				current.update_marker_type(marker_id, i);
		});
	}

	menu.addSeparator();
	QAction *act_del = menu.addAction("Delete Marker");

	QAction *selected = menu.exec(table_view_->viewport()->mapToGlobal(pos));
	if (!guard || !selected)
		return;

	if (selected == act_copy_tc) {
		std::string tc = marker.active_timecode(fps);
		QGuiApplication::clipboard()->setText(QString::fromStdString(tc));
	} else if (selected == act_copy_memo) {
		QGuiApplication::clipboard()->setText(QString::fromStdString(marker.comment));
	} else if (selected == act_copy_yt) {
		std::string yt = YoutubeExporter::generate_chapters(session_snapshot);
		QGuiApplication::clipboard()->setText(QString::fromStdString(yt));
		controller_.notify("Copied YouTube chapters to clipboard", 2500);
	} else if (selected == act_del) {
		if (controller.session().session_id() == session_id)
			controller.delete_marker(marker.id);
	}
}

void DockWidget::onMarkerAdded(const MemoMarker &marker, int)
{
	table_model_->add_marker(marker, controller_.session().frame_rate());
	table_view_->scrollToBottom();
	update_status_ui();
}

void DockWidget::onMarkerUpdated(const MemoMarker &marker, int row)
{
	table_model_->update_marker(marker, row);
}

void DockWidget::onMarkerRemoved(uint32_t id, int row)
{
	table_model_->remove_marker(id, row);
	update_status_ui();
}

void DockWidget::onMarkersReset(const std::vector<MemoMarker> &markers, const VideoFrameRate &fps)
{
	table_model_->set_markers(markers, fps);
	update_status_ui();
}

void DockWidget::onSessionStarted(const QString &)
{
	update_status_ui();
}

void DockWidget::onSessionPaused()
{
	update_status_ui();
}

void DockWidget::onSessionResumed()
{
	update_status_ui();
}

void DockWidget::onSessionStopped(const QString &)
{
	update_status_ui();
}

void DockWidget::onVideoPathResolved(const QString &)
{
	update_status_ui();
}

void DockWidget::onFocusMemoInputRequested()
{
	if (auto *dock = qobject_cast<QDockWidget *>(parentWidget())) {
		dock->show();
		dock->raise();
		dock->activateWindow();
	}
	edit_memo_->setFocus();
	edit_memo_->selectAll();
}
