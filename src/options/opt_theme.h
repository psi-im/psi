#ifndef OPT_THEME_H
#define OPT_THEME_H

#include "optionstab.h"
#include <QPointer>

class QModelIndex;
class QSortFilterProxyModel;
class QWidget;
class PsiThemeModel;
class PsiThemeProvider;
class QDialog;

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

protected slots:
	void modelRowsInserted(const QModelIndex &parent, int first, int last);
	void showThemeScreenshot();

private slots:
	void themeSelected(const QModelIndex &current, const QModelIndex &previous);
	void startLoading();
private:
	QString getThemeId(const QString &objName) const;

private:
	QWidget *w = nullptr;
	PsiThemeModel *unsortedModel = nullptr;
	QSortFilterProxyModel *themesModel = nullptr;
	PsiThemeProvider *provider = nullptr;
	QPointer<QDialog> screenshotDialog;
};

#endif
