/*
 * contactlistview.h - base contact list widget class
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

#ifndef CONTACTLISTVIEW_H
#define CONTACTLISTVIEW_H

#include "hoverabletreeview.h"

#include <QPointer>

class ContactListItem;
class ContactListItemMenu;
class ContactListModel;
class QLineEdit;
class QWidget;

class ContactListView : public HoverableTreeView {
    Q_OBJECT

public:
    ContactListView(QWidget *parent = nullptr);

    ContactListModel *realModel() const;
    QModelIndexList   realIndexes(const QModelIndexList &indexes) const;
    QModelIndex       realIndex(const QModelIndex &index) const;
    QModelIndexList   proxyIndexes(const QModelIndexList &indexes) const;
    QModelIndex       proxyIndex(const QModelIndex &index) const;
    ContactListItem * itemProxy(const QModelIndex &index) const;
    void              setEditingIndex(const QModelIndex &index, bool editing) const;

    void activate(const QModelIndex &index);
    void toggleExpandedState(const QModelIndex &index);
    void ensureVisible(const QModelIndex &index);

    // reimplemented
    void setModel(QAbstractItemModel *model);

public slots:
    virtual void rename();

signals:
    void realExpanded(const QModelIndex &);
    void realCollapsed(const QModelIndex &);
    void modelItemsUpdated();

protected:
    // reimplemented
    bool viewportEvent(QEvent *event);
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint);

    virtual void showToolTip(const QModelIndex &index, const QPoint &globalPos) const;

protected slots:
    virtual void itemActivated(const QModelIndex &index);

    virtual void showOfflineChanged();
    void         updateGroupExpandedState();

    // reimplamented
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    virtual void focusInEvent(QFocusEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);

protected:
    // reimplamented
    void contextMenuEvent(QContextMenuEvent *);
    void drawBranches(QPainter *, const QRect &, const QModelIndex &) const;
    void keyPressEvent(QKeyEvent *);
    void resizeEvent(QResizeEvent *);
    void rowsInserted(const QModelIndex &parent, int start, int end) override;

    QLineEdit *currentEditor() const;

    bool                         isContextMenuVisible();
    virtual ContactListItemMenu *createContextMenuFor(ContactListItem *item) const;

    virtual void addContextMenuActions();
    virtual void removeContextMenuActions();
    virtual void addContextMenuAction(QAction *action);
    virtual void updateContextMenu();

private:
    QPointer<ContactListItemMenu> contextMenu_;
    bool                          contextMenuActive_;
};

#endif // CONTACTLISTVIEW_H
