#include "marker-table-model.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QPainter>

MarkerTableModel::MarkerTableModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

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
			return QString::number(m.id);
		case Col_Timecode:
			return QString::fromStdString(m.active_timecode(fps_));
		case Col_Frame:
			return QString::number(m.frame_index);
		case Col_Label:
			return QString::fromStdString(m.label);
		case Col_Comment:
			return QString::fromStdString(m.comment);
		case Col_Status:
			return m.is_paused ? QString("Paused") : QString("Normal");
		default:
			return QVariant();
		}
	} else if (role == Qt::ForegroundRole) {
		if (index.column() == Col_Status && m.is_paused) {
			return QBrush(QColor("#e67e22")); // Orange for paused
		}
	} else if (role == Qt::DecorationRole) {
		if (index.column() == Col_Label) {
			// カラーバッジの描画
			QPixmap pix(12, 12);
			pix.fill(Qt::transparent);
			QPainter painter(&pix);
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setBrush(QColor(QString::fromStdString(m.color)));
			painter.setPen(Qt::NoPen);
			painter.drawEllipse(0, 0, 12, 12);
			return pix;
		}
	} else if (role == Qt::TextAlignmentRole) {
		if (index.column() == Col_Id || index.column() == Col_Frame || index.column() == Col_Status) {
			return static_cast<int>(Qt::AlignCenter);
		}
		if (index.column() == Col_Timecode) {
			return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
		}
	}

	return QVariant();
}

QVariant MarkerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal)
		return QVariant();

	if (role == Qt::DisplayRole) {
		switch (section) {
		case Col_Id:
			return QString("#");
		case Col_Timecode:
			return QString("Timecode");
		case Col_Frame:
			return QString("Frame");
		case Col_Label:
			return QString("Type");
		case Col_Comment:
			return QString("Memo / Comment");
		case Col_Status:
			return QString("Status");
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
	if (!index.isValid() || role != Qt::EditRole || index.row() < 0 || index.row() >= static_cast<int>(markers_.size()))
		return false;

	auto &m = markers_[index.row()];
	if (index.column() == Col_Comment) {
		m.comment = value.toString().toStdString();
		emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
		emit markerCommentChanged(m.id, value.toString());
		return true;
	} else if (index.column() == Col_Label) {
		m.label = value.toString().toStdString();
		emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
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
	fps_ = fps;
	int row = static_cast<int>(markers_.size());
	beginInsertRows(QModelIndex(), row, row);
	markers_.push_back(marker);
	endInsertRows();
}

void MarkerTableModel::update_marker(const MemoMarker &marker)
{
	for (size_t i = 0; i < markers_.size(); ++i) {
		if (markers_[i].id == marker.id) {
			markers_[i] = marker;
			QModelIndex left = index(static_cast<int>(i), 0);
			QModelIndex right = index(static_cast<int>(i), Col_Count - 1);
			emit dataChanged(left, right);
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
