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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
		Theme *t = provider->load(id);
		ThemeItemInfo ti;
		if (t) { // if loaded
			ti.id = id;
			ti.title = t->title();
			ti.screenshot = t->screenshot();
			ti.isValid = true;
		} else {
			ti.isValid = false;
		}
		return ti;
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
#if QT_VERSION >= 0x040600
	beginResetModel();
#endif
	while (i.hasNext()) {
		ThemeItemInfo ti = i.next();
		if (ti.isValid) {
			qDebug("%s theme loaded", qPrintable(ti.id));
			themesInfo.append(ti);
		} else {
			qDebug("failed to load theme %s", qPrintable(ti.id));
		}
	}
#if QT_VERSION >= 0x040600
	endResetModel();
#else
	reset();
#endif
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
#if QT_VERSION >= 0x040600
			beginResetModel();
#endif
			foreach (const QString id, provider->themeIds()) {
				ThemeItemInfo ti = loader(id);
				if (ti.isValid) {
					themesInfo.append(ti);
				}
			}
#if QT_VERSION >= 0x040600
			endResetModel();
#else
			reset();
#endif
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
