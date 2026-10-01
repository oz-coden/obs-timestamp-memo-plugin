#include "json-exporter.hpp"
#include "session-codec.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

bool JsonExporter::export_to_file(const RecordingSession &session, const std::string &output_path)
{
	QFileInfo fi(QString::fromStdString(output_path));
	fi.dir().mkpath(".");

	QJsonObject root = SessionCodec::encode(session);
	QJsonDocument doc(root);

	QSaveFile file(QString::fromStdString(output_path));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}

	QByteArray data = doc.toJson(QJsonDocument::Indented);
	if (file.write(data) != data.size()) {
		file.cancelWriting();
		return false;
	}

	return file.commit();
}
