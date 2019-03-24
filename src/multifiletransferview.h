/*
 * multifiletransferview.h - file transfer delegate
 * Copyright (C) 2019 Sergey Ilinykh
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

#ifndef MULTIFILETRANSFERVIEW_H
#define MULTIFILETRANSFERVIEW_H

#include <QStyledItemDelegate>

class MultiFileTransferDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    // cache for sizeHint
    mutable int fontPixelSize = 0;
    mutable int itemHeight;
    mutable int spacing;
    mutable int textLeft;
    mutable int textTop;
    mutable int speedTop;
    mutable int progressTop;
    mutable int progressHeight;
    mutable int addButtonHeight;
    mutable QRect iconRect;
    mutable QPixmap progressTexture;

    static void niceUnit(qlonglong n, qlonglong *div, QString *unit);

    static QString roundedNumber(qlonglong n, qlonglong div);

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option, const QModelIndex &index) override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
protected:
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

};


#endif // MULTIFILETRANSFERVIEW_H
