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
#include "psiiconset.h"


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
			ti.isCurrent = provider->current() && provider->current()->id() == id;
		} else {
			ti.title = ":-(";
		}

		qDebug("%s theme loading status: %s", qPrintable(theme->id()), ti.isValid? "success" : "failure");
		return ti;
	}

	void asyncLoad(const QString &id, std::function<void(const ThemeItemInfo&)> loadCallback)
	{
		Theme *currentTheme = provider->current();
		Theme *theme = provider->theme(id);
		if (!theme || !theme->load([this, theme, currentTheme, loadCallback](bool success) {
			qDebug("%s theme loading status: %s", qPrintable(theme->id()), success? "success" : "failure");
			// TODO invent something smarter

			ThemeItemInfo ti;
			if (success) { // if loaded
				ti.id = theme->id();
				ti.title = theme->title();
				ti.screenshot = theme->screenshot();
				ti.isValid = true;
				ti.isCurrent = currentTheme && (currentTheme->id() == ti.id);
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
	connect(&themeWatcher, SIGNAL(resultReadyAt(int)),
			SLOT(onThreadedResultReadyAt(int)));
	connect(&themeWatcher, SIGNAL(finished()), SLOT(loadComplete()));
}

void PsiThemeModel::onThreadedResultReadyAt(int index)
{
	ThemeItemInfo ti = themeWatcher.resultAt(index);
	if (ti.isValid) {
		beginInsertRows(QModelIndex(), themesInfo.size(), themesInfo.size());
		themesInfo.append(ti);
		endInsertRows();
	}
}

void PsiThemeModel::loadComplete()
{
	qDebug("Themes loading finished");
}

void PsiThemeModel::setType(const QString &type)
{
	providerType = type;
	PsiThemeProvider *provider = PsiThemeManager::instance()->provider(type);
	if (provider) {
		Loader loader = Loader(provider);
		if (provider->threadedLoading()) {
			themesFuture = QtConcurrent::mapped(provider->themeIds(), loader);
			themeWatcher.setFuture(themesFuture);
		} else {
			QStringList ids = provider->themeIds();
			qDebug() << ids;
			foreach (const QString &id, ids) {
				loader.asyncLoad(id, [this](const ThemeItemInfo &ti) {
					if (ti.isValid) {
						beginInsertRows(QModelIndex(), themesInfo.size(), themesInfo.size());
						themesInfo.append(ti);
						endInsertRows();
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
		case Qt::DecorationRole:
			return IconsetFactory::icon(QString("clients/")+themesInfo[index.row()].id.section('/',0, 0)).pixmap();
		case IdRole:
			return themesInfo[index.row()].id;
		case Qt::DisplayRole:
		case TitleRole:
			return themesInfo[index.row()].title;
		case ScreenshotRole:
		{
			QPixmap p;
			p.loadFromData(themesInfo[index.row()].screenshot);
			return p;
		}
		case IsCurrent:
			return themesInfo[index.row()].isCurrent;
	}
	return QVariant();
}

bool PsiThemeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	PsiThemeProvider *provider = PsiThemeManager::instance()->provider(providerType);
	if (!index.isValid() || !provider) {
		return false;
	}

	if (role == IsCurrent) {
		if (value.toBool()) {
			provider->setCurrentTheme(themesInfo[index.row()].id);
			return true;
		} else {
			// TODO set fallback / default
		}
	}

	return false;
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
