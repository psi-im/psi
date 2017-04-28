/*
 * psithememodel.h - just a model for theme views
 * Copyright (C) 2010 Rion (Sergey Ilinyh)
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

#include "psithememodel.h"

#include <QtConcurrentMap>
#include <QPixmap>

#include "psithememanager.h"


class PsiThemeModel;
struct PsiThemeModel::Loader
{
	Loader(PsiThemeProvider *provider_)
	: provider(provider_) { }

	typedef ThemeItemInfo result_type;

	ThemeItemInfo operator()(const QString &id)
	{
		ThemeItemInfo ti;
		ti.id = id;
		Theme *theme = provider->theme(id);
		if (theme && theme->load()) {
			ti.title = theme->title();
			ti.screenshot = theme->screenshot();
			ti.isValid = true;
		}

		qDebug("%s theme loading status: %s", qPrintable(theme->id()), ti.isValid? "success" : "failure");
		return ti;
	}

	void asyncLoad(const QString &id, std::function<void(const ThemeItemInfo&)> loadCallback)
	{
		Theme *theme = provider->theme(id);
		if (!theme || !theme->load([this, theme, loadCallback](bool success) {
			qDebug("%s theme loading status: %s", qPrintable(theme->id()), success? "success" : "failure");
			// TODO invent something smarter

		    ThemeItemInfo ti;
			if (success) { // if loaded
				ti.id = theme->id();
				ti.title = theme->title();
				ti.screenshot = theme->screenshot();
				ti.isValid = true;
			}
		    loadCallback(ti);
		})) {
			loadCallback(ThemeItemInfo());
		}
	}

	PsiThemeProvider *provider;
};

//------------------------------------------------------------------------------
// PsiThemeModel
//------------------------------------------------------------------------------

PsiThemeModel::PsiThemeModel(QObject *parent)
	: QAbstractListModel(parent)
{
	connect(&themeWatcher, SIGNAL(progressValueChanged(int)),
			SLOT(loadProgress(int)));
	connect(&themeWatcher, SIGNAL(finished()), SLOT(loadComplete()));
}

void PsiThemeModel::loadProgress(int pv)
{
	qDebug("%d", pv);
}

void PsiThemeModel::loadComplete()
{
	QFutureIterator<ThemeItemInfo> i(themesFuture);
	beginResetModel();
	while (i.hasNext()) {
		ThemeItemInfo ti = i.next();
		if (ti.isValid) {
			qDebug("%s theme loaded", qPrintable(ti.id));
			themesInfo.append(ti);
		} else {
			qDebug("failed to load theme %s", qPrintable(ti.id));
		}
	}
	endResetModel();
}

void PsiThemeModel::setType(const QString &type)
{
	PsiThemeProvider *provider = PsiThemeManager::instance()->provider(type);
	if (provider) {
		Loader loader = Loader(provider);
		if (provider->threadedLoading()) {
			themesFuture = QtConcurrent::mapped(provider->themeIds(), loader);
			themeWatcher.setFuture(themesFuture);
		} else {
			foreach (const QString &id, provider->themeIds()) {
				loader.asyncLoad(id, [this](const ThemeItemInfo &ti) {
					if (ti.isValid) {
						beginInsertRows(QModelIndex(), themesInfo.size(), themesInfo.size());
						//beginResetModel(); // FIXME make proper model update
						themesInfo.append(ti);
						endInsertRows();
						//endResetModel();
					}
				});

			}
		}
	}
}

int PsiThemeModel::rowCount ( const QModelIndex & parent ) const
{
	Q_UNUSED(parent)
	return themesInfo.count();
}

QVariant PsiThemeModel::data ( const QModelIndex & index, int role ) const
{
	switch (role) {
		case IdRole:
			return themesInfo[index.row()].id;
		case TitleRole:
			return themesInfo[index.row()].title;
		case ScreenshotRole:
			QPixmap p;
			p.loadFromData(themesInfo[index.row()].screenshot);
			return p;
	}
	return QVariant();
}


int PsiThemeModel::themeRow(const QString &id)
{
	int i = 0;
	foreach (const ThemeItemInfo &tii, themesInfo) {
		if (tii.id == id) {
			return i;
		}
		i++;
	}
	return -1;
}
