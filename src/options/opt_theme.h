#ifndef OPT_THEME_H
#define OPT_THEME_H

#include "optionstab.h"
#include <QPixmap>

class QModelIndex;
class QSortFilterProxyModel;
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

protected slots:
	void modelRowsInserted(const QModelIndex &parent, int first, int last);

private slots:
	void themeSelected(const QModelIndex &current, const QModelIndex &previous);
	void startLoading();
private:
	QString getPixmapBytes(const QPixmap &image) const;

private:
	QWidget *w = nullptr;
	PsiThemeModel *unsortedModel = nullptr;
	QSortFilterProxyModel *themesModel = nullptr;
	PsiThemeProvider *provider = nullptr;
};

#endif
