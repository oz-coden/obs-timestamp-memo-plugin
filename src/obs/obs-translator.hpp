#pragma once
#include <QTranslator>

// Adapts this plugin's Qt translation contexts to the module's OBS locale.
// English source text is the quoted locale key. Other applications' contexts
// are deliberately left to their own translators.
class ObsTranslator : public QTranslator {
public:
	ObsTranslator();
	~ObsTranslator() override;
	bool isEmpty() const override { return false; }
	QString translate(const char *context, const char *source, const char *, int) const override;
};
