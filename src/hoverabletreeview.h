/*
 * hoverabletreeview.h - QTreeView that allows to show hovered items apart
 * Copyright (C) 2008-2010  Yandex LLC (Michail Pishchagin)
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

#ifndef HOVERABLETREEVIEW_H
#define HOVERABLETREEVIEW_H

#include <QStyleOption>
#include <QTreeView>

typedef QStyleOptionViewItem HoverableStyleOptionViewItemBaseClass;

class HoverableStyleOptionViewItem : public HoverableStyleOptionViewItemBaseClass {
public:
    enum StyleOptionVersion { Version = HoverableStyleOptionViewItemBaseClass::Version + 1 };

    bool   hovered;
    QPoint hoveredPosition;

    HoverableStyleOptionViewItem();
    HoverableStyleOptionViewItem(const HoverableStyleOptionViewItem &other) :
        HoverableStyleOptionViewItemBaseClass(Version)
    {
        *this = other;
    }
    HoverableStyleOptionViewItem(const QStyleOptionViewItem &other);
    HoverableStyleOptionViewItem &       operator=(const QStyleOptionViewItem &other);
    inline HoverableStyleOptionViewItem &operator=(const HoverableStyleOptionViewItem &other)
    {
        return *this = static_cast<QStyleOptionViewItem>(other);
    }

protected:
    HoverableStyleOptionViewItem(int version);
};

class HoverableTreeView : public QTreeView {
    Q_OBJECT

public:
    HoverableTreeView(QWidget *parent = nullptr);

    enum HoverableItemFeature { Hovered = 0x8000 };

    void repairMouseTracking();

protected:
    // reimplemented
    void mouseMoveEvent(QMouseEvent *event);
    void drawRow(QPainter *painter, const QStyleOptionViewItem &options, const QModelIndex &index) const;
    void startDrag(Qt::DropActions supportedActions);

    QPoint mousePosition() const;

private:
    QPoint mousePosition_;
};

#endif // HOVERABLETREEVIEW_H
