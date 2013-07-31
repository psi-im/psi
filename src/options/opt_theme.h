#ifndef OPT_THEME_H
#define OPT_THEME_H

#include "optionstab.h"

class QWidget;
class PsiThemeModel;
class PsiThemeProvider;

class OptionsTabAppearanceThemes : public MetaOptionsTab
{
	Q_OBJECT
public:
	OptionsTabAppearanceThemes(QObject *parent);
};

class OptionsTabAppearanceTheme : public OptionsTab
{
	Q_OBJECT
public:
	OptionsTabAppearanceTheme(QObject *parent, PsiThemeProvider *provider_);
	~OptionsTabAppearanceTheme();

	bool stretchable() const { return true; }
	QWidget *widget();
	void applyOptions();
	void restoreOptions();

private:
	QWidget *w;
	PsiThemeModel *themesModel;
	PsiThemeProvider *provider;
};

#endif
