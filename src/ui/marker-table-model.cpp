#include "marker-table-model.hpp"

#include <QBrush>
#include <QColor>

MarkerTableModel::MarkerTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int MarkerTableModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return static_cast<int>(markers_.size());
}

int MarkerTableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return Col_Count;
}

QVariant MarkerTableModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(markers_.size()))
		return QVariant();

	const auto &m = markers_[index.row()];

	if (role == Qt::DisplayRole || role == Qt::EditRole) {
		switch (index.column()) {
		case Col_Id:
			return static_cast<uint>(m.id);
		case Col_Timecode:
			return QString::fromStdString(m.active_timecode(fps_));
		case Col_Frame:
			return static_cast<qulonglong>(m.frame_index);
		case Col_Label:
			return QString::fromStdString(m.label);
		case Col_Comment:
			return QString::fromStdString(m.comment);
		case Col_Status:
			return m.is_paused ? tr("PAUSED") : tr("REC");
		default:
			return QVariant();
		}
	} else if (role == Qt::ForegroundRole) {
		if (index.column() == Col_Status) {
			if (m.is_paused) {
				return QBrush(QColor("#f39c12"));
			} else {
				return QBrush(QColor("#2ecc71"));
			}
		}
	} else if (role == Qt::BackgroundRole) {
		if (index.column() == Col_Label && !m.color.empty()) {
			QColor c(QString::fromStdString(m.color));
			if (c.isValid()) {
				c.setAlpha(45);
				return QBrush(c);
			}
		}
	} else if (role == Qt::TextAlignmentRole) {
		switch (index.column()) {
		case Col_Id:
		case Col_Frame:
		case Col_Status:
			return static_cast<int>(Qt::AlignCenter);
		case Col_Timecode:
			return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
		case Col_Label:
		case Col_Comment:
			return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
		default:
			return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
		}
	}

	return QVariant();
}

QVariant MarkerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch (section) {
		case Col_Id:
			return "#";
		case Col_Timecode:
			return tr("Timecode");
		case Col_Frame:
			return tr("Frame");
		case Col_Label:
			return tr("Label");
		case Col_Comment:
			return tr("Comment / Memo");
		case Col_Status:
			return tr("Status");
		default:
			return QVariant();
		}
	}
	return QVariant();
}

Qt::ItemFlags MarkerTableModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	Qt::ItemFlags defaultFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	if (index.column() == Col_Comment || index.column() == Col_Label) {
		defaultFlags |= Qt::ItemIsEditable;
	}
	return defaultFlags;
}

bool MarkerTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (!index.isValid() || role != Qt::EditRole || index.row() < 0 ||
	    index.row() >= static_cast<int>(markers_.size()))
		return false;

	auto marker = markers_[index.row()];
	if (index.column() == Col_Comment) {
		marker.comment = value.toString().toStdString();
		emit markerDataChanged(marker.id, QString::fromStdString(marker.label),
				       QString::fromStdString(marker.comment));
		return true;
	} else if (index.column() == Col_Label) {
		marker.label = value.toString().toStdString();
		emit markerDataChanged(marker.id, QString::fromStdString(marker.label),
				       QString::fromStdString(marker.comment));
		return true;
	}

	return false;
}

void MarkerTableModel::set_markers(const std::vector<MemoMarker> &markers, const VideoFrameRate &fps)
{
	beginResetModel();
	markers_ = markers;
	fps_ = fps;
	endResetModel();
}

void MarkerTableModel::add_marker(const MemoMarker &marker, const VideoFrameRate &fps)
{
	int row = static_cast<int>(markers_.size());
	beginInsertRows(QModelIndex(), row, row);
	markers_.push_back(marker);
	fps_ = fps;
	endInsertRows();
}

void MarkerTableModel::update_marker(const MemoMarker &marker, int row)
{
	if (row >= 0 && row < static_cast<int>(markers_.size()) && markers_[row].id == marker.id) {
		markers_[row] = marker;
		emit dataChanged(index(row, 0), index(row, Col_Count - 1));
		return;
	}

	for (size_t i = 0; i < markers_.size(); ++i) {
		if (markers_[i].id == marker.id) {
			markers_[i] = marker;
			emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), Col_Count - 1));
			break;
		}
	}
}

void MarkerTableModel::remove_marker(uint32_t id, int row)
{
	if (row >= 0 && row < static_cast<int>(markers_.size()) && markers_[row].id == id) {
		beginRemoveRows(QModelIndex(), row, row);
		markers_.erase(markers_.begin() + row);
		endRemoveRows();
		return;
	}

	for (size_t i = 0; i < markers_.size(); ++i) {
		if (markers_[i].id == id) {
			int r = static_cast<int>(i);
			beginRemoveRows(QModelIndex(), r, r);
			markers_.erase(markers_.begin() + i);
			endRemoveRows();
			break;
		}
	}
}

void MarkerTableModel::clear()
{
	beginResetModel();
	markers_.clear();
	endResetModel();
}
