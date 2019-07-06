/*
 * psithememodel.h - just a model for theme views
 * Copyright (C) 2010-2017 Sergey Ilinykh
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PSITHEMEMODEL_H
#define PSITHEMEMODEL_H

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QStringList>

class Theme;
class PsiThemeProvider;

struct ThemeItemInfo
{
    QString id;
    QString title;
    QString version;
    QString description;
    QStringList authors;
    QString creation;
    QString homeUrl;

    bool hasPreview;
    bool isValid = false;
    bool isCurrent = false;
};


class PsiThemeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ThemeRoles {
        IdRole = Qt::UserRole + 1,
        HasPreviewRole,
        TitleRole,
        IsCurrent
    };

    PsiThemeModel(PsiThemeProvider *provider, QObject *parent);
    ~PsiThemeModel();

    int rowCount ( const QModelIndex & parent = QModelIndex() ) const ;
    QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
    int themeRow(const QString &id);

    bool setData(const QModelIndex &index, const QVariant &value, int role);

public slots:
    void load();

private slots:
    void onThreadedResultReadyAt(int index);
    void loadComplete();

private:
    struct Loader;
    Loader *loader = nullptr;
    PsiThemeProvider *provider = nullptr;
    QFutureWatcher<ThemeItemInfo> themeWatcher;
    QFuture<ThemeItemInfo> themesFuture;
    QList<ThemeItemInfo> themesInfo;
};

#endif
