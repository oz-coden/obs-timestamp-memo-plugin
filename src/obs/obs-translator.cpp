#include "obs-translator.hpp"
#include <obs-module.h>
#include <QCoreApplication>
#include <cstring>

ObsTranslator::ObsTranslator()
{
	QCoreApplication::installTranslator(this);
}
ObsTranslator::~ObsTranslator()
{
	QCoreApplication::removeTranslator(this);
}
QString ObsTranslator::translate(const char *context, const char *source, const char *, int) const
{
	if (!context || !source)
		return {};
	bool owned = false;
	for (const auto *name : {"TimestampMemo", "DockWidget", "ExportDialog", "SettingsDialog", "MarkerTableModel"})
		owned = owned || std::strcmp(context, name) == 0;
	if (!owned)
		return {};
	QByteArray key(source);
	// OBS preserves escape sequences in lookup keys (only values are decoded).
	key.replace("\n", "\\n");
	key.replace("\r", "\\r");
	key.replace("\t", "\\t");
	key.replace("\"", "\\\"");
	const auto *text = obs_module_text(key.constData());
	return text && std::strcmp(text, key.constData()) != 0 ? QString::fromUtf8(text) : QString();
}
