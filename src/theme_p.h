#ifndef THEME_P_H
#define THEME_P_H

#include <QSharedData>
#include "theme.h"

class QWidget;

class ThemePrivate : public QSharedData
{
public:
	PsiThemeProvider *provider;
	Theme::State state = Theme::NotLoaded;

	// metadata
	QString id, name, version, description, creation, homeUrl;
	QStringList authors;
	QHash<QString, QString> info;

	// runtime info
	QString filepath;
	bool caseInsensitiveFS;

public:
	ThemePrivate(PsiThemeProvider *provider);
	virtual ~ThemePrivate();

	virtual bool exists() = 0;
	virtual bool load(); // synchronous load
	virtual bool load(std::function<void(bool)> loadCallback);  // asynchronous load

	virtual bool hasPreview() const;
	virtual QWidget* previewWidget(); // this hack must be replaced with something widget based

	QByteArray loadData(const QString &fileName) const;
	Theme::ResourceLoader *resourceLoader();
};

#endif // THEME_P_H
