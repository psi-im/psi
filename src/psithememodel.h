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

#ifndef PSITHEMEMODEL_H
#define PSITHEMEMODEL_H

#include <QAbstractListModel>
#include <QFutureWatcher>

class Theme;

struct ThemeItemInfo
{
	QString id;
	QString title;
	QByteArray screenshot;
	bool isValid;
};


class PsiThemeModel : public QAbstractListModel
{
	Q_OBJECT

public:
	enum ThemeRoles {
		IdRole = Qt::UserRole + 1,
		ScreenshotRole = Qt::UserRole + 2,
		TitleRole = Qt::UserRole + 3
	};

	PsiThemeModel(QObject *parent);
	void setType(const QString &type);

	int rowCount ( const QModelIndex & parent = QModelIndex() ) const ;
	QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
	int themeRow(const QString &id);

private slots:
	void loadProgress(int);
	void loadComplete();

private:
	class Loader;
	QFutureWatcher<ThemeItemInfo> themeWatcher;
	QFuture<ThemeItemInfo> themesFuture;
	QList<ThemeItemInfo> themesInfo;
};

#endif
