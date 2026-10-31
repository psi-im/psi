/*
 * discoitems_p.h - private Service Discovery tree items
 * Copyright (C) 2003  Michail Pishchagin
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

#ifndef DISCOITEMS_P_H
#define DISCOITEMS_P_H

#include <QIcon>
#include <QPointer>
#include <QTreeWidget>
#include <functional>
#include <iris/xmpp_tasks.h>

using namespace XMPP;
class TaskList;
constexpr int MoreItemsType = QTreeWidgetItem::UserType;

class DiscoListItem;

class DiscoConnector : public QObject {
    Q_OBJECT
public:
    DiscoConnector(QObject *parent) : QObject(parent) { }

signals:
    void itemUpdated(QTreeWidgetItem *);

private:
    friend class DiscoListItem;
};

struct DiscoData {
    QPointer<Task>                                rootTask;
    std::function<bool()>                         autoItemsEnabled, autoInfoEnabled;
    std::function<int()>                          itemsPerPage;
    std::function<QIcon(const DiscoItem &, bool)> itemIcon;
    std::function<void(const QString &)>          showError;
    TaskList                                     *tasks;
    DiscoConnector                               *d;
    void                                          cancelTasks();
};

//----------------------------------------------------------------------------
// DiscoBaseItem
//----------------------------------------------------------------------------
class DiscoBaseItem : public QObject, public QTreeWidgetItem {
public:
    DiscoBaseItem(QTreeWidget *parent, int type = Type);
    DiscoBaseItem(QTreeWidgetItem *parent, int type = Type);
    ~DiscoBaseItem() { }

    virtual const DiscoItem &item() const;
    virtual void             itemSelected() { }
    virtual void             itemClicked() { }

    virtual bool operator<(const QTreeWidgetItem &other) const;
};

//----------------------------------------------------------------------------
// DiscoExtraItem
//----------------------------------------------------------------------------

class DiscoListItem;
class DiscoExtraItem : public DiscoBaseItem {
public:
    DiscoExtraItem(DiscoListItem *parent, const QString &text);
    virtual ~DiscoExtraItem() { }

    virtual void itemClicked();

private:
    void init(const QString &text);

private:
    DiscoListItem *_parent;
};

//----------------------------------------------------------------------------
// DiscoListItem
//----------------------------------------------------------------------------

class DiscoListItem : public DiscoBaseItem {
    Q_OBJECT
public:
    DiscoListItem(DiscoItem it, DiscoData *d, QTreeWidget *parent);
    DiscoListItem(DiscoItem it, DiscoData *d, QTreeWidgetItem *parent, bool isDeifinitelyLeaf = false);
    ~DiscoListItem();

    static DiscoListItem *fromItem(QTreeWidgetItem *item);
    void                  setExpanded(bool expand);

    const DiscoItem &item() const;

    void itemSelected();

public slots: // the two are used internally by class, and also called by DiscoDlg::Private::refresh()
    void    updateInfo(bool automatic = false);
    void    updateItems(bool parentAutoItems = false, bool more = false);
    QString getErrorInfo() const;

private slots:
    void discoItemsFinished();
    void discoInfoFinished();

private:
    DiscoItem                di;
    DiscoData               *d;
    bool                     isRoot;
    bool                     alreadyItems, alreadyInfo;
    bool                     autoItems; // used in updateItemsFinished
    bool                     autoInfo;
    bool                     isDefinitelyLeaf = false;
    QString                  errorInfo;
    QPointer<DiscoExtraItem> moreItem;
    QPointer<JT_DiscoItems>  itemsTask;
    QPointer<JT_DiscoInfo>   infoTask;
    bool                     hasMoreItems     = false;
    bool                     appendItems      = false;
    bool                     interactiveItems = false;
    SubsetsClientManager     subsets;

    void copyItem(const DiscoItem &);
    void updateInfo(const DiscoItem &);
    void updateItemsFinished(const DiscoList &);
    void autoItemsChildren() const; // automatically call disco#items for children :-)

    // helper functions
    void init(DiscoItem it, DiscoData *dd);

    void hideChildIndicator();

    bool autoItemsEnabled() const;
    bool autoInfoEnabled() const;
    int  itemsPerPage() const;
};

QString discoItemToolTip(const DiscoItem &item, const QString &errorInfo, const QStringList &identityIcons = { });

#endif // DISCOITEMS_P_H
