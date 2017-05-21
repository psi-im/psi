/*
 * opt_theme.h
 * Copyright (C) 2010-2017  Sergey Ilinykh, Vitaly Tonkacheyev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

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
