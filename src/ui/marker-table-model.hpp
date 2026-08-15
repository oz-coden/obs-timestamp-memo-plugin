#pragma once

#include "memo-marker.hpp"
#include "timecode-helper.hpp"

#include <vector>
#include <QAbstractTableModel>
#include <QModelIndex>
#include <QVariant>

class MarkerTableModel : public QAbstractTableModel {
	Q_OBJECT

public:
	enum Column { Col_Id = 0, Col_Timecode, Col_Frame, Col_Label, Col_Comment, Col_Status, Col_Count };

	explicit MarkerTableModel(QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

	void set_markers(const std::vector<MemoMarker> &markers, const VideoFrameRate &fps);
	void add_marker(const MemoMarker &marker, const VideoFrameRate &fps);
	void update_marker(const MemoMarker &marker);
	void clear();

	const std::vector<MemoMarker> &get_markers() const { return markers_; }

signals:
	void markerCommentChanged(uint32_t marker_id, const QString &comment);

private:
	std::vector<MemoMarker> markers_;
	VideoFrameRate fps_{60, 1};
};
