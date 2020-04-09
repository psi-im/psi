/*
 * optionstreemodel.h
 * Copyright (C) 2006  Remko Troncon
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

#ifndef OPTIONSTREEMODEL_H
#define OPTIONSTREEMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QStack>
#include <QVariant>

class OptionsTree;

class OptionsTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    OptionsTreeModel(OptionsTree *tree, QObject *parent = nullptr);

    // Reimplemented from QAbstractItemModel
    int           rowCount(const QModelIndex &parent = QModelIndex()) const;
    int           columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant      data(const QModelIndex &index, int role) const;
    bool          setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    QVariant      headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QModelIndex   index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex   parent(const QModelIndex &index) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    // custom accessors
    QString indexToOptionName(QModelIndex idx) const;

public slots:
    bool setFlat(bool);

protected:
    enum Section { Name = 0, Type = 1, Value = 2, Comment = 3, SectionBound = 4 };
    QString     getParentName(const QString &option) const;
    QModelIndex index(const QString &option, Section sec = Name) const;
    int         nameToIndex(QString name) const;
    bool        internalNode(QString name) const;
    QStringList getFlatOptions() const;

protected slots:
    void optionChanged(const QString &option);
    void optionAboutToBeInserted(const QString &option);
    void optionInserted(const QString &option);
    void optionAboutToBeRemoved(const QString &option);
    void optionRemoved(const QString &option);

private:
    OptionsTree *               tree_;
    bool                        flat_;
    mutable QHash<int, QString> indexMap; // option index (seq number) to option.name mapping
    mutable QHash<QString, int> nameMap;  // option.name to option index mapping
    mutable int                 nextIdx;
    QStack<bool>                realRemove;
    mutable QStringList         flatCache;
};

#endif // OPTIONSTREEMODEL_H
